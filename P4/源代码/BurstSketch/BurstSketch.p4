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
    apply(tbl_stage2_bucket_hash);
    apply(tbl_stage1_bucket_hash);
    apply(tbl_cur_time_window_update);

    apply(tbl_last_time_window_update);                 // stage 1 

    apply(tbl_logic_time_window_get);                   // stage 1

    if (ig_intr_md.ingress_port != 68) {
        apply(tbl_cell1_flow_key_read);                 // stage 1  

        if (mdata.flow_key != mdata.cell1_flow_key) {
            apply(tbl_cell2_flow_key_read);             // stage 4

            if (mdata.flow_key != mdata.cell2_flow_key) {  
                // stage 2 search completely, continue to search stage 1
                apply(tbl_stage1_flow_key_update);      // stage 7

                if (mdata.flow_key != mdata.stage1_flow_key) {
                    apply(tbl_stage1_value_sub);        // stage 8
                } else {
                    apply(tbl_stage1_value_add);        // stage 8
                }

                apply(tbl_forward);                     // stage 9
            } else {
                if (mdata.logic_time_window_idx == 0) {
                    apply(tbl_cell2_window1_update);    // stage 5
                } else if (mdata.logic_time_window_idx == 1) {
                    apply(tbl_cell2_window2_update);    // stage 6
                }
            }

        } else {
            if (mdata.logic_time_window_idx == 0) {
                apply(tbl_cell1_window1_update);        // stage 2
            } else if (mdata.logic_time_window_idx == 1) {
                apply(tbl_cell1_window2_update);        // stage 3
            }
        }
    } else {  // second access
        if (user.stage1_value == 50) {
            apply(tbl_cell1_flow_key_update);           // stage 1   
            
            if (mdata.flow_key != mdata.cell1_flow_key) {
                if (mdata.logic_time_window_idx == 0) {
                    apply(tbl_cell1_window1_read);      // stage 2
                } else if (mdata.logic_time_window_idx == 1) {
                    apply(tbl_cell1_window2_read);      // stage 3
                }
                apply(tbl_cell2_flow_key_update);       // stage 4

                if (mdata.flow_key != mdata.cell2_flow_key) {
                    // read cur window value 
                    if (mdata.logic_time_window_idx == 0) {
                        apply(tbl_cell2_window1_read);  // stage 5
                    } else if (mdata.logic_time_window_idx == 1) {
                        apply(tbl_cell2_window2_read);  // stage 6
                    }
                    // recir
                } else {
                    apply(tbl_cell2_window1_replace);   // stage 5
                    apply(tbl_cell2_window2_replace);   // stage 6
                }
            } else {
                apply(tbl_cell1_window1_replace);       // stage 2
                apply(tbl_cell1_window2_replace);       // stage 3
            }         
        }
        apply(tbl_stage1_flow_key_clear);               // stage 7 
        apply(tbl_stage1_value_clear);                  // stage 8
    }
}



/*==================== Egress Pipeline ====================*/
control egress {
}