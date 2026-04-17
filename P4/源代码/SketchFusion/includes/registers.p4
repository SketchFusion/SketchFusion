#define SKETCH_SIZE 65536



/*==================== Sketch Row1 Flow Key Register Definition ====================*/
register reg_row1_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row1_flow_key_update {
    reg: reg_row1_flow_key;

    condition_lo: register_lo == 0;
    condition_hi: register_hi == mdata.flow_key;

    update_lo_1_predicate: condition_lo or condition_hi;
    update_lo_1_value: register_lo + 1;
    
    update_hi_1_predicate: condition_lo or condition_hi;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.row1_flow_key;
    output_value: register_lo;
}

action act_row1_flow_key_update() {
    regact_row1_flow_key_update.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 1
table tbl_row1_flow_key_update {
    actions {
        act_row1_flow_key_update;
    }
    default_action: act_row1_flow_key_update;
}

blackbox stateful_alu regact_row1_flow_key_replace {
    reg: reg_row1_flow_key;

    update_hi_1_value: mdata.flow_key;
}

action act_row1_flow_key_replace() {
    regact_row1_flow_key_replace.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 1
table tbl_row1_flow_key_replace {
    actions {
        act_row1_flow_key_replace;
    }
    default_action: act_row1_flow_key_replace;
}



/*==================== Sketch Row1 Task Register Definition ====================*/
register reg_row1_task {
    width: 16;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row1_task_read {
    reg: reg_row1_task;

    output_dst: mdata.row1_task;
    output_value: register_lo;
}

action act_row1_task_read() {
    regact_row1_task_read.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 2
table tbl_row1_task_read {
    actions {
        act_row1_task_read;
    }
    default_action: act_row1_task_read;
}

blackbox stateful_alu regact_row1_task_replace {
    reg: reg_row1_task;

    update_hi_1_value: user.task;
}

action act_row1_task_replace() {
    regact_row1_task_replace.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 2
table tbl_row1_task_replace {
    actions {
        act_row1_task_replace;
    }
    default_action: act_row1_task_replace;
}



/*==================== Sketch Row1 Value Register Definition ====================*/
register reg_row1_value {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row1_value_read {
    reg: reg_row1_value;

    output_dst: mdata.row1_value;
    output_value: register_lo;
}

action act_row1_value_read() {
    regact_row1_value_read.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 3
table tbl_row1_value_read {
    actions {
        act_row1_value_read;
    }
    default_action: act_row1_value_read;
}

blackbox stateful_alu regact_row1_value_replace_1 {
    reg: reg_row1_value;

    update_lo_1_value: 1;
}

action act_row1_value_replace_1() {
    regact_row1_value_replace_1.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 3 
table tbl_row1_value_replace_1 {
    actions {
        act_row1_value_replace_1;
    }
    default_action: act_row1_value_replace_1;
}

blackbox stateful_alu regact_row1_value_replace_2 {
    reg: reg_row1_value;

    condition_lo: user.task == 2;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 50;   // value comes from the stage 1 of BurstSketch
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: mdata.aion_frequency;   // value comes from the frequency calculation of AionSketch
}

action act_row1_value_replace_2() {
    regact_row1_value_replace_2.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 3
table tbl_row1_value_replace_2 {
    actions {
        act_row1_value_replace_2;
    }
    default_action: act_row1_value_replace_2;
}



/*==================== Sketch Row1 Extra Register1 Definition ====================*/
register reg_row1_extra1 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row1_extra1_replace_1 {
    reg: reg_row1_extra1;

    update_lo_1_value: 0;
}

action act_row1_extra1_replace_1() {
    regact_row1_extra1_replace_1.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 7
table tbl_row1_extra1_replace_1 {
    actions {
        act_row1_extra1_replace_1;
    }
    default_action: act_row1_extra1_replace_1;
}

blackbox stateful_alu regact_row1_extra1_replace_2 {
    reg: reg_row1_extra1;

    update_lo_1_value: mdata.aion_period;   // value comes from the period calculation of AionSketch
}

action act_row1_extra1_replace_2() {
    regact_row1_extra1_replace_2.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 7
table tbl_row1_extra1_replace_2 {
    actions {
        act_row1_extra1_replace_2;
    }
    default_action: act_row1_extra1_replace_2;
}


/*==================== Sketch Row1 Extra Register2 Definition ====================*/
register reg_row1_extra2 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row1_extra2_replace {
    reg: reg_row1_extra2;

    update_lo_1_value: 0;   // set the flag to false
}

action act_row1_extra2_replace() {
    regact_row1_extra2_replace.execute_stateful_alu(mdata.row1_loc);
}

@pragma stage 8
table tbl_row1_extra2_replace {
    actions {
        act_row1_extra2_replace;
    }
    default_action: act_row1_extra2_replace;
}




/*==================== Sketch Row2 Flow Key Register Definition ====================*/
register reg_row2_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row2_flow_key_update {
    reg: reg_row2_flow_key;

    condition_lo: register_lo == 0;
    condition_hi: register_hi == mdata.flow_key;

    update_lo_1_predicate: condition_lo or condition_hi;
    update_lo_1_value: register_lo + 1;
    
    update_hi_1_predicate: condition_lo or condition_hi;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.row2_flow_key;
    output_value: register_lo;
}

action act_row2_flow_key_update() {
    regact_row2_flow_key_update.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 4
table tbl_row2_flow_key_update {
    actions {
        act_row2_flow_key_update;
    }
    default_action: act_row2_flow_key_update;
}

blackbox stateful_alu regact_row2_flow_key_replace {
    reg: reg_row2_flow_key;

    update_hi_1_value: mdata.flow_key;
}

action act_row2_flow_key_replace() {
    regact_row2_flow_key_replace.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 4
table tbl_row2_flow_key_replace {
    actions {
        act_row2_flow_key_replace;
    }
    default_action: act_row2_flow_key_replace;
}



/*==================== Sketch Row2 Task Register Definition ====================*/
register reg_row2_task {
    width: 16;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row2_task_read {
    reg: reg_row2_task;

    output_dst: mdata.row2_task;
    output_value: register_lo;
}

action act_row2_task_read() {
    regact_row2_task_read.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 5
table tbl_row2_task_read {
    actions {
        act_row2_task_read;
    }
    default_action: act_row2_task_read;
}

blackbox stateful_alu regact_row2_task_replace {
    reg: reg_row2_task;

    update_hi_1_value: mdata.row2_task;
}

action act_row2_task_replace() {
    regact_row2_task_replace.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 5
table tbl_row2_task_replace {
    actions {
        act_row2_task_replace;
    }
    default_action: act_row2_task_replace;
}



/*==================== Sketch Row2 Value Register Definition ====================*/
register reg_row2_value {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row2_value_read {
    reg: reg_row2_value;

    output_dst: mdata.row2_value;
    output_value: register_lo;
}

action act_row2_value_read() {
    regact_row2_value_read.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 6
table tbl_row2_value_read {
    actions {
        act_row2_value_read;
    }
    default_action: act_row2_value_read;
}

blackbox stateful_alu regact_row2_value_replace_1 {
    reg: reg_row2_value;

    update_lo_1_value: 1;
}

action act_row2_value_replace_1() {
    regact_row2_value_replace_1.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 6
table tbl_row2_value_replace_1 {
    actions {
        act_row2_value_replace_1;
    }
    default_action: act_row2_value_replace_1;
}

blackbox stateful_alu regact_row2_value_replace_2 {
    reg: reg_row2_value;

    condition_lo: user.task == 2;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 50;   // value comes from the stage 1 of BurstSketch
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: mdata.aion_frequency;   // value comes from the frequency calculation of AionSketch
}

action act_row2_value_replace_2() {
    regact_row2_value_replace_2.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 6
table tbl_row2_value_replace_2 {
    actions {
        act_row2_value_replace_2;
    }
    default_action: act_row2_value_replace_2;
}


/*==================== AionSketch Filter Part ====================*/
register reg_aion_sketch_filter_count {
    width: 16;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_aion_sketch_filter_count {
    reg: reg_aion_sketch_filter_count;

    output_dst: mdata.aion_filter_count;
    output_value: register_lo;
}

action act_aion_sketch_filter_count() {
    regact_aion_sketch_filter_count.execute_stateful_alu(mdata.aion_filter_loc);
}

@pragma stage 1
table tbl_aion_sketch_filter_count {
    actions {
        act_aion_sketch_filter_count;
    }
    default_action: act_aion_sketch_filter_count;
}

register reg_aion_sketch_filter_sum {
    width: 16;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_aion_sketch_filter_sum {
    reg: reg_aion_sketch_filter_sum;

    output_dst: mdata.aion_filter_sum;
    output_value: register_lo;
}

action act_aion_sketch_filter_sum() {
    regact_aion_sketch_filter_sum.execute_stateful_alu(mdata.aion_filter_loc);
}

@pragma stage 1
table tbl_aion_sketch_filter_sum {
    actions {
        act_aion_sketch_filter_sum;
    }
    default_action: act_aion_sketch_filter_sum;
}


/*==================== BurstSketch Stage1 Part ====================*/
register reg_burst_sketch_stage1 {
    width: 32;
    instance_count: SKETCH_SIZE;
}



/*==================== Sketch Row2 Extra Register1 Definition ====================*/
register reg_row2_extra1 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row2_extra1_replace_1 {
    reg: reg_row2_extra1;

    update_lo_1_value: 0;
}

action act_row2_extra1_replace_1() {
    regact_row2_extra1_replace_1.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 9
table tbl_row2_extra1_replace_1 {
    actions {
        act_row2_extra1_replace_1;
    }
    default_action: act_row2_extra1_replace_1;
}

blackbox stateful_alu regact_row2_extra1_replace_2 {
    reg: reg_row2_extra1;

    update_lo_1_value: mdata.aion_period;   // value comes from the period calculation of AionSketch
}

action act_row2_extra1_replace_2() {
    regact_row2_extra1_replace_2.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 9
table tbl_row2_extra1_replace_2 {
    actions {
        act_row2_extra1_replace_2;
    }
    default_action: act_row2_extra1_replace_2;
}



/*==================== Sketch Row2 Extra Register2 Definition ====================*/
register reg_row2_extra2 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

blackbox stateful_alu regact_row2_extra2_replace {
    reg: reg_row2_extra2;

    update_lo_1_value: 0;   // set the flag to false
}

action act_row2_extra2_replace() {
    regact_row2_extra2_replace.execute_stateful_alu(mdata.row2_loc);
}

@pragma stage 10
table tbl_row2_extra2_replace {
    actions {
        act_row2_extra2_replace;
    }
    default_action: act_row2_extra2_replace;
}