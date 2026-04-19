// nop action
action act_nop() {}


// get min of row1.value and row2.value
action act_min_row1_row2() {
    min(mdata.min_row_value, mdata.row1_value, mdata.row2_value);
}

// get min of min_val and row3.value
// action act_min_minmal_row3() {
//     min(mdata.min_row_value, mdata.min_row_value, mdata.row3_value);
// }

// set min_row_idx, min_row_value, and min_row_stable to row 1
action act_set_min_row1() {
    modify_field(mdata.min_row_idx, 1);
    modify_field(mdata.min_row_value, mdata.row1_value);
    modify_field(mdata.min_row_stable, mdata.row1_stable);
}

// set min_row_idx, min_row_value, and min_row_stable to row 2
action act_set_min_row2() {
    modify_field(mdata.min_row_idx, 2);
    modify_field(mdata.min_row_value, mdata.row2_value);
    modify_field(mdata.min_row_stable, mdata.row2_stable);
}

// set min_row_idx, min_row_value, and min_row_stable to row 3
// action act_set_min_row3() {
//     modify_field(mdata.min_row_idx, 3);
//     modify_field(mdata.min_row_value, mdata.row3_value);
//     modify_field(mdata.min_row_stable, mdata.row3_stable);
// }

// recirculate and set minimal sketch row index
action act_recir() {
    modify_field(user.min_row_idx, mdata.min_row_idx);
    modify_field(user.m_rand, mdata.m_rand);
    modify_field(user.m_probability, mdata.m_probability);
    
    recirculate(68);
}


// set metadata field and generate random number 
// action act_replace() {
//     modify_field(mdata.min_row_idx, user.min_row_idx);
//     modify_field(mdata.min_row_value, user.min_row_value);
//     modify_field(mdata.min_row_stable, user.min_row_stable);

//     modify_field_rng_uniform(mdata.m_rand, 0, 255);
// }

// generate random number 
action act_rand() {
    modify_field_rng_uniform(mdata.m_rand, 0, 255);
}

// calcualte probability
action act_probability(probability) {
    modify_field(mdata.m_probability, probability);
}

// whether can replace
action act_can_replace() {
    modify_field(mdata.min_row_idx, user.min_row_idx);
    subtract(mdata.can_replace, user.m_rand, user.m_probability);
}