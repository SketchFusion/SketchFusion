#define SKETCH_SIZE 65536

/*==================== Current Time Window ====================*/
// register definition for current time window
// low 32 bits store the start time of the current window
// high 32 bits store the current window number
register reg_cur_time_window {
    width: 64;
    instance_count: 1;
}

// register action definition for current time window
// Truncate the upper 32 bits of the timestamp 
// each time the register is updated by 1, the accumulated time increases by 65 ms
@pragma stateful_field_slice ig_intr_md.ingress_mac_tstamp 47 16
blackbox stateful_alu regact_cur_time_window_update {
    reg: reg_cur_time_window;

    condition_lo: register_lo - ig_intr_md.ingress_mac_tstamp >= 15;  // ≈ 1s 

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: register_lo + 15;

    update_hi_1_predicate: condition_lo;
    update_hi_1_value: register_hi + 1;

    output_dst: mdata.cur_time_window;
    output_value: alu_hi;
}

// action definition for current time window update
action act_cur_time_window_update() {
    regact_cur_time_window_update.execute_stateful_alu(0);
}

// table definition for current time window update
@pragma stage 0
table tbl_cur_time_window_update {
    actions {
        act_cur_time_window_update;
    }
    default_action: act_cur_time_window_update;
}



/*==================== Filter Part Count ====================*/
register reg_filter_count {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_filter_count_read {
    reg: reg_filter_count;

    output_dst: mdata.filter_count;
    output_value: register_lo;
}

action act_filter_count_read() {
    regact_filter_count_read.execute_stateful_alu(mdata.filter_loc);
}

@pragma stage 1
table tbl_filter_count_read {
    actions {
        act_filter_count_read;
    }
    default_action: act_filter_count_read;
}



/*==================== Filter Part Sum ====================*/
register reg_filter_sum {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_filter_sum_read {
    reg: reg_filter_sum;

    output_dst: mdata.filter_sum;
    output_value: register_lo;
}

action act_filter_sum_read() {
    regact_filter_sum_read.execute_stateful_alu(mdata.filter_loc);
}

@pragma stage 1
table tbl_filter_sum_read {
    actions {
        act_filter_sum_read;
    }
    default_action: act_filter_sum_read;
}



/*==================== Sketch Row1 For Flow Key ====================*/
register reg_sketch_row1_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_sketch_row1_flow_key_update {
    reg: reg_sketch_row1_flow_key;

    condition_lo: register_lo == 0;
    condition_hi: register_hi == mdata.flow_key;

    update_lo_1_predicate: condition_lo or condition_hi;
    update_lo_1_value: register_lo + 1;

    update_hi_1_predicate: condition_lo or condition_hi;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.sketch_row1_flow_key;
    output_value: alu_hi;
}

action act_sketch_row1_flow_key_update() {
    regact_sketch_row1_flow_key_update.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 3
table tbl_sketch_row1_flow_key_update {
    actions {
        act_sketch_row1_flow_key_update;
    }
    default_action: act_sketch_row1_flow_key_update;
}

blackbox stateful_alu regact_sketch_row1_flow_key_replace {
    reg: reg_sketch_row1_flow_key;

    update_lo_1_value: mdata.flow_key;
}

action act_sketch_row1_flow_key_replace() {
    regact_sketch_row1_flow_key_replace.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 3
table tbl_sketch_row1_flow_key_replace {
    actions {
        act_sketch_row1_flow_key_replace;
    }
    default_action: act_sketch_row1_flow_key_replace;
}



/*==================== Sketch Row1 For Period ====================*/
register reg_sketch_row1_period {
    width: 16;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_sketch_row1_period_update {
    reg: reg_sketch_row1_period;

    condition_lo: mdata.report_period > register_lo;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: mdata.report_period;

    update_hi_1_predicate: condition_lo;
    update_hi_1_value: 1;
    update_hi_2_predicate: not condition_lo;
    update_hi_2_value: 0;

    output_dst: mdata.is_new;
    output_value: alu_hi;
}

action act_sketch_row1_period_update() {
    regact_sketch_row1_period_update.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 4
table tbl_sketch_row1_period_update {
    actions {
        act_sketch_row1_period_update;
    }
    default_action: act_sketch_row1_period_update;
}

blackbox stateful_alu regact_sketch_row1_period_replace {
    reg: reg_sketch_row1_period;

    update_lo_1_value: mdata.report_period;
}

action act_sketch_row1_period_replace() {
    regact_sketch_row1_period_replace.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 4
table tbl_sketch_row1_period_replace {
    actions {
        act_sketch_row1_period_replace;
    }
    default_action: act_sketch_row1_period_replace;
}


/*==================== Sketch Row1 For Frequency ====================*/
register reg_sketch_row1_frequency {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_sketch_row1_frequency_calc1 {
    reg: reg_sketch_row1_frequency;

    update_lo_1_value: register_lo + 1;
}

action act_sketch_row1_frequency_calc1() {
    regact_sketch_row1_frequency_calc1.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 5
table tbl_sketch_row1_frequency_calc1 {
    actions {
        act_sketch_row1_frequency_calc1;
    }
    default_action: act_sketch_row1_frequency_calc1;
}

blackbox stateful_alu regact_sketch_row1_frequency_calc2 {
    reg: reg_sketch_row1_frequency;

    condition_lo: register_lo == 0;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: mdata.report_frequency + 1;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: mdata.report_frequency;
}

action act_sketch_row1_frequency_calc2() {
    regact_sketch_row1_frequency_calc2.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 5
table tbl_sketch_row1_frequency_calc2 {
    actions {
        act_sketch_row1_frequency_calc2;
    }
    default_action: act_sketch_row1_frequency_calc2;
}

blackbox stateful_alu regact_sketch_row1_frequency_read {
    reg: reg_sketch_row1_frequency;

    output_dst: mdata.sketch_row1_frequency;
    output_value: register_lo;
}

action act_sketch_row1_frequency_read() {
    regact_sketch_row1_frequency_read.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 5
table tbl_sketch_row1_frequency_read {
    actions {
        act_sketch_row1_frequency_read;
    }
    default_action: act_sketch_row1_frequency_read;
}

blackbox stateful_alu regact_sketch_row1_frequency_replace {
    reg: reg_sketch_row1_frequency;

    update_lo_1_value: 0;
}

action act_sketch_row1_frequency_replace() {
    regact_sketch_row1_frequency_replace.execute_stateful_alu(mdata.sketch_row1_loc);
}

@pragma stage 5
table tbl_sketch_row1_frequency_replace {
    actions {
        act_sketch_row1_frequency_replace;
    }
    default_action: act_sketch_row1_frequency_replace;
}



/*==================== Sketch Row2 For Flow Key ====================*/
register reg_sketch_row2_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_sketch_row2_flow_key_update {
    reg: reg_sketch_row2_flow_key;

    condition_lo: register_lo == 0;
    condition_hi: register_hi == mdata.flow_key;

    update_lo_1_predicate: condition_lo or condition_hi;
    update_lo_1_value: register_lo + 1;

    update_hi_1_predicate: condition_lo or condition_hi;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.sketch_row2_flow_key;
    output_value: alu_hi;
}

action act_sketch_row2_flow_key_update() {
    regact_sketch_row2_flow_key_update.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 6
table tbl_sketch_row2_flow_key_update {
    actions {
        act_sketch_row2_flow_key_update;
    }
    default_action: act_sketch_row2_flow_key_update;
}

blackbox stateful_alu regact_sketch_row2_flow_key_replace {
    reg: reg_sketch_row2_flow_key;

    update_lo_1_value: mdata.flow_key;
}

action act_sketch_row2_flow_key_replace() {
    regact_sketch_row2_flow_key_replace.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 6
table tbl_sketch_row2_flow_key_replace {
    actions {
        act_sketch_row2_flow_key_replace;
    }
    default_action: act_sketch_row2_flow_key_replace;
}



/*==================== Sketch Row2 For Period ====================*/
register reg_sketch_row2_period {
    width: 16;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_sketch_row2_period_update {
    reg: reg_sketch_row2_period;

    condition_lo: mdata.report_period > register_lo;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: mdata.report_period;

    update_hi_1_predicate: condition_lo;
    update_hi_1_value: 1;
    update_hi_2_predicate: not condition_lo;
    update_hi_2_value: 0;

    output_dst: mdata.is_new;
    output_value: alu_hi;
}

action act_sketch_row2_period_update() {
    regact_sketch_row2_period_update.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 7
table tbl_sketch_row2_period_update {
    actions {
        act_sketch_row2_period_update;
    }
    default_action: act_sketch_row2_period_update;
}

blackbox stateful_alu regact_sketch_row2_period_replace {
    reg: reg_sketch_row2_period;

    update_lo_1_value: mdata.report_period;
}

action act_sketch_row2_period_replace() {
    regact_sketch_row2_period_replace.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 7
table tbl_sketch_row2_period_replace {
    actions {
        act_sketch_row2_period_replace;
    }
    default_action: act_sketch_row2_period_replace;
}


/*==================== Sketch Row2 For Frequency ====================*/
register reg_sketch_row2_frequency {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_sketch_row2_frequency_calc1 {
    reg: reg_sketch_row2_frequency;

    update_lo_1_value: register_lo + 1;
}

action act_sketch_row2_frequency_calc1() {
    regact_sketch_row2_frequency_calc1.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 8
table tbl_sketch_row2_frequency_calc1 {
    actions {
        act_sketch_row2_frequency_calc1;
    }
    default_action: act_sketch_row2_frequency_calc1;
}

blackbox stateful_alu regact_sketch_row2_frequency_calc2 {
    reg: reg_sketch_row2_frequency;

    condition_lo: register_lo == 0;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: mdata.report_frequency + 1;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: mdata.report_frequency;
}

action act_sketch_row2_frequency_calc2() {
    regact_sketch_row2_frequency_calc2.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 8
table tbl_sketch_row2_frequency_calc2 {
    actions {
        act_sketch_row2_frequency_calc2;
    }
    default_action: act_sketch_row2_frequency_calc2;
}

blackbox stateful_alu regact_sketch_row2_frequency_read {
    reg: reg_sketch_row2_frequency;

    output_dst: mdata.sketch_row2_frequency;
    output_value: register_lo;
}

action act_sketch_row2_frequency_read() {
    regact_sketch_row2_frequency_read.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 8
table tbl_sketch_row2_frequency_read {
    actions {
        act_sketch_row2_frequency_read;
    }
    default_action: act_sketch_row2_frequency_read;
}

blackbox stateful_alu regact_sketch_row2_frequency_replace {
    reg: reg_sketch_row2_frequency;

    update_lo_1_value: mdata.report_frequency;
}

action act_sketch_row2_frequency_replace() {
    regact_sketch_row2_frequency_replace.execute_stateful_alu(mdata.sketch_row2_loc);
}

@pragma stage 8
table tbl_sketch_row2_frequency_replace {
    actions {
        act_sketch_row2_frequency_replace;
    }
    default_action: act_sketch_row2_frequency_replace;
}