// nop action
action act_nop() {}

// drop
action act_drop() {
    modify_field(ig_intr_md_for_tm.drop_ctl, 1);
}

// report period update action 
action act_report_period_update(report_period) {
    modify_field(mdata.report_period, report_period);
}

// frequency calculation according to the report period and cur time window
action act_report_frequency_update(report_frequency) {
    modify_field(mdata.report_frequency, report_frequency);
}

// set sketch row1 min
action act_set_min_row1() {
    modify_field(mdata.min_row_idx, 1);
    modify_field(mdata.min_row_frequency, mdata.sketch_row1_frequency);
}

// set sketch row2 min 
action act_set_min_row2() {
    modify_field(mdata.min_row_idx, 2);
    modify_field(mdata.min_row_frequency, mdata.sketch_row2_frequency);
}

// get min of row1 and row2
action act_min_row1_row2() {
    min(mdata.min_row_frequency, mdata.sketch_row1_frequency, mdata.sketch_row2_frequency);
}

// recirculate
action act_recir() {
    modify_field(user.min_row_idx, mdata.min_row_idx);
    modify_field(user.min_row_frequency, mdata.min_row_frequency);

    recirculate(68);
}

// random number generation
action act_rand() {
    modify_field_rng_uniform(mdata.m_rand, 0, 255);
}

// probability get
action act_probability(probability) {
    modify_field(mdata.m_probability, probability);
}

// whether can replace
action act_is_replace() {
    subtract(mdata.is_replace, mdata.m_rand, mdata.m_probability);
}