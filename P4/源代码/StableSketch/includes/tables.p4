// get min of row1.value and row2.value
@pragma stage 5
table tbl_min_row1_row2 {
    actions {
        act_min_row1_row2;
    }
    default_action: act_min_row1_row2;
}

// get min of min_val and row3.value
// @pragma stage 7
// table tbl_min_minmal_row3 {
//     actions {
//         act_min_minmal_row3;
//     }
//     default_action: act_min_minmal_row3;
// }

// set min_row_idx, min_row_value, and min_row_stable to row 1
@pragma stage 4
table tbl_set_min_row1 {
    actions {
        act_set_min_row1;
    }
    default_action: act_set_min_row1;
}

// set min_row_idx, min_row_value, and min_row_stable to row 2
@pragma stage 6
table tbl_set_min_row2 {
    actions {
        act_set_min_row2;
    }
    default_action: act_set_min_row2;
}

// set min_row_idx, min_row_value, and min_row_stable to row 3
// @pragma stage 8
// table tbl_set_min_row3 {
//     actions {
//         act_set_min_row3;
//     }
//     default_action: act_set_min_row3;
// }

// recirculate and set minimal sketch row index, value and stable
@pragma stage 8
table tbl_recir {
    actions {
        act_recir;
    }
    default_action: act_recir;
}

// set mdata and generate random number 
@pragma stage 7
table tbl_rand {
    actions {
        act_rand;
    }
    default_action: act_rand;
}

// probability calculation table
@pragma stage 7
table tbl_probability {
    reads {
        mdata.min_row_value: exact;
        mdata.min_row_stable: exact;
    }
    actions {
        act_probability;
    }
    size: 256;
}

// whether can replace
@pragma stage 0
table tbl_can_replace {
    actions {
        act_can_replace;
    }
    default_action: act_can_replace;
}