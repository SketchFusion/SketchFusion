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

    if (ig_intr_md.ingress_port != 68) {  // first access
        apply(tbl_row1_flow_key_update);            // stage 1 

        if (mdata.flow_key != mdata.row1_flow_key) {
            apply(tbl_row1_value_read);             // stage 2
            apply(tbl_row1_stable_read);            // stage 2

            apply(tbl_row2_flow_key_update);        // stage 3

            if (mdata.flow_key != mdata.row2_flow_key) {
                apply(tbl_row2_value_read);         // stage 4
                apply(tbl_row2_stable_read);        // stage 4

                apply(tbl_set_min_row1);            // stage 4
                apply(tbl_min_row1_row2);           // stage 5

                if (mdata.min_row_value == mdata.row2_value) {
                    apply(tbl_set_min_row2);        // stage 6
                }

                apply(tbl_probability);             // stage 7
                apply(tbl_rand);                    // stage 7

                apply(tbl_recir);                   // stage 8
            } else {
                apply(tbl_row2_value_update);       // stage 4
                apply(tbl_row2_stable_update);      // stage 4
            }
        } else {
            apply(tbl_row1_value_update);           // stage 2
            apply(tbl_row1_stable_update);          // stage 2
        }
    } else {  // second access
        // apply(tbl_replace);  
        // apply(tbl_probability);
        apply(tbl_can_replace);                     // stage 0

        if (mdata.can_replace == 1) {   
            if (mdata.min_row_idx == 1) {
                apply(tbl_row1_flow_key_replace);   // stage 1
                apply(tbl_row1_value_replace);      // stage 2
                apply(tbl_row1_stable_replace);     // stage 2
            } else if (mdata.min_row_idx == 2) {
                apply(tbl_row2_flow_key_replace);   // stage 3
                apply(tbl_row2_value_replace);      // stage 4
                apply(tbl_row2_stable_replace);     // stage 4
            }
        }
    }
}



/*==================== Egress Pipeline ====================*/
control egress {
}