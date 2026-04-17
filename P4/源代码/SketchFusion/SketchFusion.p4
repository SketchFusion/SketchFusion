#include <tofino/intrinsic_metadata.p4>
#include <tofino/constants.p4>
#include <tofino/primitives.p4>
#include "tofino/stateful_alu_blackbox.p4"

#include "includes/headers.p4"
#include "includes/parser.p4"
#include "includes/registers.p4"
#include "includes/actions.p4"
#include "includes/tables.p4"
#include "includes/hashes.p4"



/*==================== Ingress Pipeline ====================*/
control ingress {
    // stage 0
    apply(tbl_flow_key_hash);
    apply(tbl_sketch_row1_hash);
    apply(tbl_sketch_row2_hash);
    apply(tbl_aion_sketch_filter_hash);
    apply(tbl_burst_sketch_stage1_hash);

    if (ig_intr_md.ingress_port != 68) {                    // first access
        apply(tbl_row1_flow_key_update);                    // stage 1

        if (mdata.row1_flow_key != mdata.flow_key) {
            apply(tbl_row1_task_read);                      // stage 2
            apply(tbl_row1_value_read);                     // stage 3

            apply(tbl_row2_flow_key_update);                // stage 4

            if (mdata.row2_flow_key != mdata.flow_key) {
                apply(tbl_row2_task_read);                  // stage 5
                apply(tbl_row2_value_read);                 // stage 6

                apply(tbl_recir);                           // stage 11
            }
        }
    } else {                                                // second access                  
        apply(tbl_aion_sketch_filter_count);                // stage 1, for AionSketch, read sidon sequence count 
        apply(tbl_aion_sketch_filter_sum);                  // stage 1, for AionSketch, read sidon sequence sum      
                                     
        if (mdata.min_row_idx == 1) {
            apply(tbl_row1_flow_key_replace);               // stage 1
            apply(tbl_row1_task_replace);                   // stage 2

            apply(tbl_report_frequency);                    // stage 2, for AionSketch, calculate the frequency
            apply(tbl_report_period);                       // stage 2, for AionSketch, calculate the period

            if (user.task <= 1) {
                apply(tbl_row1_value_replace_1);            // stage 3, for StableSketch or PandoraSketch, set to one 
                apply(tbl_row1_extra1_replace_1);           // stage 7, for StableSketch and PandoraSketch, set Stable and Inactivity to zero
                apply(tbl_row1_extra2_replace);             // stage 8, for PandoraSketch, set the flag to false
            } else if (user.task <= 3) {  
                apply(tbl_row1_value_replace_2);            // stage 3, for AionSketch, set the frequency; for BurstSketch, set 50
                apply(tbl_row1_extra1_replace_2);           // stage 7, for AionSketch, record the period; for BurstSketch, keep the value unchanged (last_window)
            }


        } else if (mdata.min_row_idx == 2) {
            apply(tbl_report_frequency_copy1);              // stage 2, for AionSketch, calculate the frequency
            apply(tbl_report_period_copy1);                 // stage 2, for AionSketch, calculate the period

            apply(tbl_row2_flow_key_replace);               // stage 4
            apply(tbl_row2_task_replace);                   // stage 5
            
            if (user.task <= 1) {
                apply(tbl_row2_value_replace_1);            // stage 6, for StableSketch or PandoraSketch, set to one 
                apply(tbl_row2_extra1_replace_1);           // stage 9, for StableSketch and PandoraSketch, set Stable and Inactivity to zero
                apply(tbl_row2_extra2_replace);             // stage 10, for PandoraSketch, set the flag to false
            } else if (user.task <= 3) {  
                apply(tbl_row2_value_replace_2);            // stage 6, for AionSketch, set the frequency; for BurstSketch, set 50
                apply(tbl_row2_extra1_replace_2);           // stage 9, for AionSketch, record the period; for BurstSketch, keep the value unchanged (last_window)
            }
        }
    }
}

/*==================== Egress Pipeline ====================*/
control egress {
    apply(tbl_value_percentile_map_row1);                   // stage 0    
    apply(tbl_value_percentile_map_row2);                   // stage 0

    apply(tbl_set_row1_min);                                // stage 1
    apply(tbl_minimum_row1_row2);                           // stage 2
    if (mdata.min_row_percentile == mdata.row2_percentile) {          
        apply(tbl_set_row2_min);                            // stage 3
    }

    apply(tbl_min_percentile_value_map);                    // stage 4

    apply(tbl_probability);                                 // stage 5
    apply(tbl_rand);                                        // stage 5

    apply(tbl_is_replace);                                  // stage 6

    apply(tbl_drop);                                        // stage 7
}