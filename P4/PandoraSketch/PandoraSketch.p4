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
    // apply(tbl_sketch_row3_hash);

    if (ig_intr_md.ingress_port != 68) {  // first access
        apply(tbl_row1_flag_update);                        // stage 1

        if (mdata.row1_flag == 1) {
            apply(tbl_row1_flow_key_update);                // stage 2

            if (mdata.row1_flow_key != mdata.flow_key) {
                apply(tbl_row1_value_read);                 // stage 3
                apply(tbl_row1_inactivity_read);            // stage 3

                apply(tbl_row2_flag_update);                // stage 4

                if (mdata.row2_flag == 1) {
                    apply(tbl_row2_flow_key_update);        // stage 5

                    if (mdata.row2_flow_key != mdata.flow_key) {
                        apply(tbl_row2_value_read);         // stage 6
                        apply(tbl_row2_inactivity_read);    // stage 6

                        apply(tbl_set_min_row1);            // stage 6
                        apply(tbl_min_row1_row2);           // stage 7

                        if (mdata.min_row_value == mdata.row2_value) {
                            apply(tbl_set_min_row2);        // stage 8
                        }

                        apply(tbl_probability);             // stage 9
                        apply(tbl_rand);                    // stage 9

                        apply(tbl_recir);                   // stage 10                       
                    } else {
                        apply(tbl_row2_value_update);       // stage 6
                        apply(tbl_row2_inactivity_update);  // stage 6
                    }
                }
            } else {
                apply(tbl_row1_value_update);               // stage 3
                apply(tbl_row1_inactivity_update);          // stage 3
            }
        }
    } else {  // second access
        // apply(tbl_replace);
        // apply(tbl_probability);
        apply(tbl_can_replace);                             // stage 0

        if (mdata.can_replace == 1) {
            if (mdata.min_row_idx == 1) {
                apply(tbl_row1_flag_replace);               // stage 1
                apply(tbl_row1_flow_key_replace);           // stage 2
                apply(tbl_row1_value_replace);              // stage 3
                apply(tbl_row1_inactivity_replace);         // stage 3
            } else if (mdata.min_row_idx == 2) {
                apply(tbl_row2_flag_replace);               // stage 4
                apply(tbl_row2_flow_key_replace);           // stage 5
                apply(tbl_row2_value_replace);              // stage 6
                apply(tbl_row2_inactivity_replace);         // stage 6
            }
        }
    }
}



/*==================== Egress Pipeline ====================*/
control egress {
}