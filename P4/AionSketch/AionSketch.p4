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
    apply(tbl_filter_hash);
    apply(tbl_sketch_row1_hash);
    apply(tbl_sketch_row2_hash);
    apply(tbl_cur_time_window_update);
    apply(tbl_rand);
    apply(tbl_probability);   

    apply(tbl_filter_count_read);                                     // stage 1
    apply(tbl_filter_sum_read);                                       // stage 1
    apply(tbl_is_replace);                                            // stage 1

    apply(tbl_report_period_update);                                  // stage 2
    apply(tbl_report_frequency_update);                               // stage 2         

    if (ig_intr_md.ingress_port != 68) {
        if (mdata.report_period >= 2) {                               // has period
            
            apply(tbl_sketch_row1_flow_key_update);                   // stage 3

            if (mdata.sketch_row1_flow_key != mdata.flow_key) {
                apply(tbl_sketch_row1_frequency_read);                // stage 5
                apply(tbl_sketch_row2_flow_key_update);               // stage 6

                if (mdata.sketch_row2_flow_key != mdata.flow_key) {
                    apply(tbl_sketch_row2_frequency_read);            // stage 8

                    apply(tbl_set_min_row1);                          // stage 8

                    apply(tbl_min_row1_row2);                         // stage 9

                    if (mdata.min_row_frequency == mdata.sketch_row2_frequency) {
                        apply(tbl_set_min_row2);                      // stage 10
                    }

                    apply(tbl_recir);                                 // stage 11
                } else {
                    apply(tbl_sketch_row2_period_update);             // stage 7

                    if (mdata.is_new == 0) {
                        apply(tbl_sketch_row2_frequency_calc1);       // stage 8
                    } else if (mdata.is_new == 1) {
                        apply(tbl_sketch_row2_frequency_calc2);       // stage 8
                    }
                }
            } else {
                apply(tbl_sketch_row1_period_update);                 // stage 4

                if (mdata.is_new == 0) {
                    apply(tbl_sketch_row1_frequency_calc1);           // stage 5
                } else if (mdata.is_new == 1) {
                    apply(tbl_sketch_row1_frequency_calc2);           // stage 5
                }
            }
        }
    } else {
        if (mdata.is_replace == 1) {
            if (user.min_row_idx == 1) {
                apply(tbl_sketch_row1_flow_key_replace);             // stage 3
                apply(tbl_sketch_row1_period_replace);               // stage 4
                apply(tbl_sketch_row1_frequency_replace);            // stage 5
            } else if (user.min_row_idx == 2) {
                apply(tbl_sketch_row2_flow_key_replace);             // stage 6
                apply(tbl_sketch_row2_period_replace);               // stage 7
                apply(tbl_sketch_row2_frequency_replace);            // stage 8
            }
        }
    }
}

/*==================== Egress Pipeline ====================*/
control egress {
}