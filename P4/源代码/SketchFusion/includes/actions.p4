// nop action
action act_nop() {}

// ingress drop
action act_ingress_drop() {
    modify_field(ig_intr_md_for_tm.drop_ctl, 1);
}

// egress drop
action act_egress_drop() {
    modify_field(eg_intr_md_for_oport.drop_ctl, 1);
}

// row1 value -> percentile map
action act_value_percentile_map_row1(percentile) {
    modify_field(mdata.row1_percentile, percentile);
}

// row2 value -> percentile map
action act_value_percentile_map_row2(percentile) {
    modify_field(mdata.row2_percentile, percentile);
}

// set row1 min 
action act_set_row1_min() {
    modify_field(mdata.min_row_idx, 1);
    modify_field(mdata.min_row_percentile, mdata.row1_percentile);
    modify_field(mdata.min_task, mdata.row1_task);
}

// set row2 min
action act_set_row2_min() {
    modify_field(mdata.min_row_idx, 2);
    modify_field(mdata.min_row_percentile, mdata.row2_percentile);
    modify_field(mdata.min_task, mdata.row2_task);
}

// set minimum of row1 and row2
action act_minimum_row1_row2() {
    min(mdata.min_row_percentile, mdata.row1_percentile, mdata.row2_percentile);
}

// min row percentile -> value map
action act_min_percentile_value_map(map_value) {
    modify_field(mdata.map_value, map_value);
}

// set probability
action act_probability(probability) {
    modify_field(mdata.m_probability, probability);
}

// set rand
action act_rand(rand) {
    modify_field(mdata.m_rand, rand);
}

// whether to replace
action act_is_replace() {
    subtract(user.is_replace, mdata.m_rand, mdata.m_probability);
}

// recirculate
action act_recir() {
    recirculate(68);
}

// cal the frequency for AionSketch
action act_report_frequency(aion_frequency) {
    modify_field(mdata.aion_frequency, aion_frequency);
}

// cal the period for AionSketch
action act_report_period(aion_period) {
    modify_field(mdata.aion_period, aion_period);
}