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



/*==================== Last Time Window ====================*/
// register definition for last time window
// determine whether clear current time window storage
// lower 16 bits store the last time window number
// higher 16 bits store whether start a new time window number
register reg_last_time_window {
    width: 32;
    instance_count: 1;
}

// register action definition for last time window
blackbox stateful_alu regact_last_time_window_update {
    reg: reg_last_time_window;

    condition_lo: mdata.cur_time_window != register_lo;

    update_lo_1_value: mdata.cur_time_window;

    update_hi_1_predicate: condition_lo;
    update_hi_1_value: 1;
    update_hi_2_predicate: not condition_lo;
    update_hi_2_value: 0;

    output_dst: mdata.is_window_updated;
    output_value: alu_hi;
}

// action definition for last time window update
action act_last_time_window_update() {
    regact_last_time_window_update.execute_stateful_alu(0);
}

// table definition for last time window update
@pragma stage 1
table tbl_last_time_window_update {
    actions {
        act_last_time_window_update;
    }
    default_action: act_last_time_window_update;
}



/*==================== Stage 2 Cell 1 For Flow Key ====================*/
// register definition for stage 2 cell 1 flow key
// lower 16 bits flag whether the cell is empty
// higher 16 bits store the flow key
register reg_cell1_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 2 cell 1 flow key read 
blackbox stateful_alu regact_cell1_flow_key_read {
    reg: reg_cell1_flow_key;

    output_dst: mdata.cell1_flow_key;
    output_value: register_hi;
}

// action definition for stage 2 cell 1 flow key read
action act_cell1_flow_key_read() {
    regact_cell1_flow_key_read.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 flow key read
@pragma stage 1 
table tbl_cell1_flow_key_read {
    actions {
        act_cell1_flow_key_read;
    }
    default_action: act_cell1_flow_key_read;
}

// register action definition for stage 2 cell 1 flow key update
blackbox stateful_alu regact_cell1_flow_key_update {
    reg: reg_cell1_flow_key;

    condition_lo: register_lo == 0;
    
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: register_lo + 1;
    
    update_hi_1_predicate: condition_lo;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.cell1_flow_key;
    output_value: alu_hi;
}

// action definition for stage 2 cell 1 flow key update
action act_cell1_flow_key_update() {
    regact_cell1_flow_key_update.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 flow key update
@pragma stage 1
table tbl_cell1_flow_key_update {
    actions {
        act_cell1_flow_key_update;
    }
    default_action: act_cell1_flow_key_update;
}


/*==================== Stage 2 Cell 1 For Window1 ====================*/
// register definition for stage 2 cell 1 Window1
register reg_cell1_window1 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 2 cell 1 Window1 update
blackbox stateful_alu regact_cell1_window1_update {
    reg: reg_cell1_window1;

    condition_lo: mdata.is_window_updated;
    
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 0;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + 1;
}

// action definition for stage 2 cell 1 Window1 update
action act_cell1_window1_update() {
    regact_cell1_window1_update.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 Window1 update
@pragma stage 2
table tbl_cell1_window1_update {
    actions {
        act_cell1_window1_update;
    }
    default_action: act_cell1_window1_update;
}

// register action for stage 2 cell 1 Window1 replace
blackbox stateful_alu regact_cell1_window1_replace {
    reg: reg_cell1_window1;

    condition_lo: mdata.logic_time_window_idx == 0;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 50;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: 0;
}

// action definition for stage 2 cell 1 Window1 replace
action act_cell1_window1_replace() {
    regact_cell1_window1_replace.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 Window1 replace
@pragma stage 2
table tbl_cell1_window1_replace {
    actions {
        act_cell1_window1_replace;
    }
    default_action: act_cell1_window1_replace;
}

// register action for stage 2 cell 1 Window1 read
blackbox stateful_alu regact_cell1_window1_read {
    reg: reg_cell1_window1;

    output_dst: mdata.cell1_cur_value;
    output_value: register_lo;
}

// action definition for stage 2 cell 1 Window1 read
action act_cell1_window1_read() {
    regact_cell1_window1_read.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 Window1 read
@pragma stage 2
table tbl_cell1_window1_read {
    actions {
        act_cell1_window1_read;
    }
    default_action: act_cell1_window1_read;
}



/*==================== Stage 2 Cell 1 For Window2 ====================*/
// register definition for stage 2 cell 1 Window2
register reg_cell1_window2 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 2 cell 1 Window2 update
blackbox stateful_alu regact_cell1_window2_update {
    reg: reg_cell1_window2;

    condition_lo: mdata.is_window_updated == 1;
    
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 0;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + 1;
}

// action definition for stage 2 cell 1 Window2 update
action act_cell1_window2_update() {
    regact_cell1_window2_update.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 Window2 update
@pragma stage 3
table tbl_cell1_window2_update {
    actions {
        act_cell1_window2_update;
    }
    default_action: act_cell1_window2_update;
}

// register action for stage 2 cell 1 Window2 replace
blackbox stateful_alu regact_cell1_window2_replace {
    reg: reg_cell1_window2;

    condition_lo: mdata.logic_time_window_idx == 1;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 50;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: 0;
}

// action definition for stage 2 cell 1 Window2 replace
action act_cell1_window2_replace() {
    regact_cell1_window2_replace.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 Window2 replace
@pragma stage 3
table tbl_cell1_window2_replace {
    actions {
        act_cell1_window2_replace;
    }
    default_action: act_cell1_window2_replace;
}

// register action for stage 2 cell 1 Window2 read
blackbox stateful_alu regact_cell1_window2_read {
    reg: reg_cell1_window2;

    output_dst: mdata.cell1_cur_value;
    output_value: register_lo;
}

// action definition for stage 2 cell 1 Window2 read
action act_cell1_window2_read() {
    regact_cell1_window2_read.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 1 Window2 read
@pragma stage 3
table tbl_cell1_window2_read {
    actions {
        act_cell1_window2_read;
    }
    default_action: act_cell1_window2_read;
}


/*==================== Stage 2 Cell 1 For Last Sudden Increase Window ====================*/
// register definition for stage 2 cell 1 last sudden increase window
// since it is necessary to traverse all cells in all buckets, the data plane only provides a timestamp register, while the control plane is responsible for updating it.
register reg_cell1_last_sudden_increase_window {
    width: 32;
    instance_count: SKETCH_SIZE;
}



/*==================== Stage 2 Cell 2 For Flow Key ====================*/
// register definition for stage 2 cell 2 flow key
register reg_cell2_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 2 cell 2 flow key read 
blackbox stateful_alu regact_cell2_flow_key_read {
    reg: reg_cell2_flow_key;

    output_dst: mdata.cell2_flow_key;
    output_value: register_lo;
}

// action definition for stage 2 cell 2 flow key read
action act_cell2_flow_key_read() {
    regact_cell2_flow_key_read.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 flow key read
@pragma stage 4 
table tbl_cell2_flow_key_read {
    actions {
        act_cell2_flow_key_read;
    }
    default_action: act_cell2_flow_key_read;
}

// register action definition for stage 2 cell 2 flow key replace
blackbox stateful_alu regact_cell2_flow_key_update {
    reg: reg_cell2_flow_key;

    condition_lo: register_lo == 0;
    
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: register_lo + 1;

    update_hi_1_predicate: condition_lo;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.cell2_flow_key;
    output_value: alu_hi;
}

// action definition for stage 2 cell 2 flow key update
action act_cell2_flow_key_update() {
    regact_cell2_flow_key_update.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 flow key update
@pragma stage 4
table tbl_cell2_flow_key_update {
    actions {
        act_cell2_flow_key_update;
    }
    default_action: act_cell2_flow_key_update;
}



/*==================== Stage 2 Cell 2 For Window1 ====================*/
// register definition for stage 2 cell 2 Window1
register reg_cell2_window1 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 2 cell 2 Window1 update
blackbox stateful_alu regact_cell2_window1_update {
    reg: reg_cell2_window1;

    condition_lo: mdata.is_window_updated;
    
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 0;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + 1;
}

// action definition for stage 2 cell 2 Window1 update
action act_cell2_window1_update() {
    regact_cell2_window1_update.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 Window1 update
@pragma stage 5
table tbl_cell2_window1_update {
    actions {
        act_cell2_window1_update;
    }
    default_action: act_cell2_window1_update;
}

// register action for stage 2 cell 2 Window1 replace
blackbox stateful_alu regact_cell2_window1_replace {
    reg: reg_cell2_window1;

    condition_lo: mdata.logic_time_window_idx == 0;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 50;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: 0;
}

// action definition for stage 2 cell 2 Window1 replace
action act_cell2_window1_replace() {
    regact_cell2_window1_replace.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 Window1 replace
@pragma stage 5
table tbl_cell2_window1_replace {
    actions {
        act_cell2_window1_replace;
    }
    default_action: act_cell2_window1_replace;
}

// register action for stage 2 cell 2 Window1 read
blackbox stateful_alu regact_cell2_window1_read {
    reg: reg_cell2_window1;

    output_dst: mdata.cell2_cur_value;
    output_value: register_lo;
}

// action definition for stage 2 cell 2 Window1 read
action act_cell2_window1_read() {
    regact_cell2_window1_read.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 Window1 read
@pragma stage 5
table tbl_cell2_window1_read {
    actions {
        act_cell2_window1_read;
    }
    default_action: act_cell2_window1_read;
}



/*==================== Stage 2 Cell 2 For Window2 ====================*/
// register definition for stage 2 cell 2 Window2
register reg_cell2_window2 {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 2 cell 2 Window2 update
blackbox stateful_alu regact_cell2_window2_update {
    reg: reg_cell2_window2;

    condition_lo: mdata.is_window_updated == 1;
    
    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 0;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo + 1;
}

// action definition for stage 2 cell 2 Window2 update
action act_cell2_window2_update() {
    regact_cell2_window2_update.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 Window2 update
@pragma stage 6
table tbl_cell2_window2_update {
    actions {
        act_cell2_window2_update;
    }
    default_action: act_cell2_window2_update;
}

// register action for stage 2 cell 2 Window2 replace
blackbox stateful_alu regact_cell2_window2_replace {
    reg: reg_cell2_window2;

    condition_lo: mdata.logic_time_window_idx == 1;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 50;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: 0;
}

// action definition for stage 2 cell 2 Window2 replace
action act_cell2_window2_replace() {
    regact_cell2_window2_replace.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 Window2 replace
@pragma stage 6
table tbl_cell2_window2_replace {
    actions {
        act_cell2_window2_replace;
    }
    default_action: act_cell2_window2_replace;
}

// register action for stage 2 cell 2 Window2 read
blackbox stateful_alu regact_cell2_window2_read {
    reg: reg_cell2_window2;

    output_dst: mdata.cell2_cur_value;
    output_value: register_lo;
}

// action definition for stage 2 cell 2 Window2 read
action act_cell2_window2_read() {
    regact_cell2_window2_read.execute_stateful_alu(mdata.stage2_loc);
}

// table definition for stage 2 cell 2 Window2 read
@pragma stage 6
table tbl_cell2_window2_read {
    actions {
        act_cell2_window2_read;
    }
    default_action: act_cell2_window2_read;
}



/*==================== Stage 2 Cell 2 For Last Sudden Increase Window ====================*/
// register definition for stage 2 cell 2 last sudden increase window
// since it is necessary to traverse all cells in all buckets, the data plane only provides a timestamp register, while the control plane is responsible for updating it.
register reg_cell2_last_sudden_increase_window {
    width: 32;
    instance_count: SKETCH_SIZE;
}



/*==================== Stage 1 For Flow Key Update and Clear ====================*/
// register definition for stage 1 flow key
register reg_stage1_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 1 flow key update
blackbox stateful_alu regact_stage1_flow_key_update {
    reg: reg_stage1_flow_key;

    condition_lo: register_lo == 0;  // is this bucket empty?
    condition_hi: register_hi == mdata.flow_key;  // does this bucket match the flow key?

    update_lo_1_predicate: condition_lo or condition_hi;
    update_lo_1_value: register_lo + 1;

    update_hi_1_predicate: condition_lo or condition_hi;
    update_hi_1_value: mdata.flow_key;

    output_dst: mdata.stage1_flow_key;
    output_value: alu_hi;
}

// action definition for stage 1 flow key update
action act_stage1_flow_key_update() {
    regact_stage1_flow_key_update.execute_stateful_alu(mdata.stage1_loc);
}

// table definition for stage 1 flow key update
@pragma stage 7
table tbl_stage1_flow_key_update {
    actions {
        act_stage1_flow_key_update;
    }
    default_action: act_stage1_flow_key_update;
}

// register action definition for stage 1 flow key clear
blackbox stateful_alu regact_stage1_flow_key_clear {
    reg: reg_stage1_flow_key;

    update_lo_1_value: 0;
    update_hi_1_value: 0;
}

// action definition for stage 1 flow key clear
action act_stage1_flow_key_clear() {
    regact_stage1_flow_key_clear.execute_stateful_alu(mdata.stage1_loc);
}

// table definition for stage 1 flow key clear
@pragma stage 7
table tbl_stage1_flow_key_clear {
    actions {
        act_stage1_flow_key_clear;
    }
    default_action: act_stage1_flow_key_clear;
}



/*==================== Stage 1 For Value Update ====================*/
// register definition for stage 1 value
register reg_stage1_value {
    width: 16;
    instance_count: SKETCH_SIZE;
}

// register action definition for stage 1 value add
blackbox stateful_alu regact_stage1_value_add {
    reg: reg_stage1_value;

    condition_lo: register_lo < 50;  // stage 1 threshold

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: register_lo + 1;

    output_value: register_lo;
    output_dst: mdata.stage1_value;
}

// action definition for stage 1 value add
action act_stage1_value_add() {
    regact_stage1_value_add.execute_stateful_alu(mdata.stage1_loc);
}

// table definition for stage 1 value add
@pragma stage 8
table tbl_stage1_value_add {
    actions {
        act_stage1_value_add;
    }
    default_action: act_stage1_value_add;
}

// register action definition for stage 1 value sub
blackbox stateful_alu regact_stage1_value_sub {
    reg: reg_stage1_value;

    condition_lo: register_lo > 0;  

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: register_lo - 1;

    output_value: register_lo;
    output_dst: mdata.stage1_value;
}

// action definition for stage 1 value sub
action act_stage1_value_sub() {
    regact_stage1_value_sub.execute_stateful_alu(mdata.stage1_loc);
}

// table definition for stage 1 value sub
@pragma stage 8
table tbl_stage1_value_sub {
    actions {
        act_stage1_value_sub;
    }
    default_action: act_stage1_value_sub;
}

blackbox stateful_alu regact_stage1_value_clear {
    reg: reg_stage1_value;

    update_lo_1_value: 0;
}

action act_stage1_value_clear() {
    regact_stage1_value_clear.execute_stateful_alu(mdata.stage1_loc);
}

@pragma stage 8
table tbl_stage1_value_clear {
    actions {
        act_stage1_value_clear;
    }
    default_action: act_stage1_value_clear;
}