// get min of row1.value and row2.value
table tbl_min_row1_row2 {
    actions {
        act_min_row1_row2;
    }
    default_action: act_min_row1_row2;
}

// get min of min_val and row3.value
// table tbl_min_minmal_row3 {
//     actions {
//         act_min_minmal_row3;
//     }
//     default_action: act_min_minmal_row3;
// }

// set min_row_idx, min_row_value, and min_row_inactivity to row 1
table tbl_set_min_row1 {
    actions {
        act_set_min_row1;
    }
    default_action: act_set_min_row1;
}

// set min_row_idx, min_row_value, and min_row_inactivity to row 2
table tbl_set_min_row2 {
    actions {
        act_set_min_row2;
    }
    default_action: act_set_min_row2;
}

// set min_row_idx, min_row_value, and min_row_inactivity to row 3
// table tbl_set_min_row3 {
//     actions {
//         act_set_min_row3;
//     }
//     default_action: act_set_min_row3;
// }

// set mdata and generate random number 
// table tbl_replace {
//     actions {
//         act_replace;
//     }
//     default_action: act_replace;
// }

// generate random number
table tbl_rand {
    actions {
        act_rand;
    }
    default_action: act_rand;
}

// probability calculation table
table tbl_probability {
    reads {
        mdata.min_row_value: exact;
        mdata.min_row_inactivity: exact;
    }
    actions {
        act_probability;
    }
    size: 256;
}


// recirculate and set minimal sketch row index, value and inactivity
table tbl_recir {
    reads {
        mdata.min_row_flag: exact;
    }
    actions {
        act_drop;
        act_recir;
    }
    size: 2;
}

// whether can replace
table tbl_can_replace {
    actions {
        act_can_replace;
    }
    default_action: act_can_replace;
}