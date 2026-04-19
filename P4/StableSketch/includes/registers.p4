#define SKETCH_SIZE 65536

/*==================== Flowkey Bucket Array For Sketch Row 1 ====================*/
// register definition
register reg_row1_flow_key {
    width: 32;  // lower 16 bits indicate whether null, upper 16 bits store the flow key.
    instance_count: SKETCH_SIZE;
}

// register action for sketch row 1 flow key update
blackbox stateful_alu regact_row1_flow_key_update {
    reg: reg_row1_flow_key;

    condition_lo: register_lo == 0;  // is this bucket empty?
    condition_hi: register_hi == mdata.flow_key; // does this bucket store the flow?

    update_lo_1_predicate: condition_lo or condition_hi; // it's empty or hit
    update_lo_1_value: register_lo + 1; // note that any non-zero value indicates non-null
    // update_lo_2_predicate: not condition_lo and not condition_hi;  // else read
    // update_lo_2_value: register_lo;  // no change

    update_hi_1_predicate: condition_lo or condition_hi;  // it's empty or hit
    update_hi_1_value: mdata.flow_key;  // write the flow key
    // update_hi_2_predicate: not condition_lo and not condition_hi;  // else read
    // update_hi_2_value: register_hi;  // no change

    output_value: alu_hi;  // read flow key to check whether update successfully
    output_dst: mdata.row1_flow_key;

    initial_register_lo_value : 0;
    initial_register_hi_value : 0;
}

// action definition for sketch row 1 flow key update
action act_row1_flow_key_update() {
    regact_row1_flow_key_update.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 flow key update
@pragma stage 1
table tbl_row1_flow_key_update {
    actions {
        act_row1_flow_key_update;
    }
    default_action: act_row1_flow_key_update;
}


// register definition for sketch row 1 flow key update
blackbox stateful_alu regact_row1_flow_key_replace {
    reg: reg_row1_flow_key;

    update_hi_1_value: mdata.flow_key;
}

// register action for sketch row 1 flow key replace
action act_row1_flow_key_replace() {
    regact_row1_flow_key_replace.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 flow key replace
@pragma stage 1
table tbl_row1_flow_key_replace {
    actions {
        act_row1_flow_key_replace;
    }
    default_action: act_row1_flow_key_replace;
}



/*==================== Value Bucket Array For Sketch Row 1 ====================*/
// register definition for sketch row 1 value
register reg_row1_value {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action for sketch row 1 value update
blackbox stateful_alu regact_row1_value_update {
    reg: reg_row1_value;

    update_lo_1_value: register_lo + 1;
}

// action definition for sketch row 1 value update
action act_row1_value_update() {
    regact_row1_value_update.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 value update
@pragma stage 2
table tbl_row1_value_update {
    actions {
        act_row1_value_update;
    }
    default_action: act_row1_value_update;
}

// register action for sketch row 1 value read
blackbox stateful_alu regact_row1_value_read {
    reg: reg_row1_value;

    output_value: register_lo;
    output_dst: mdata.row1_value;
}

// action definition for sketch row 1 value read
action act_row1_value_read() {
    regact_row1_value_read.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 value read
@pragma stage 2
table tbl_row1_value_read {
    actions {
        act_row1_value_read;
    }
    default_action: act_row1_value_read;
}

// register definition for sketch row 1 value replace
blackbox stateful_alu regact_row1_value_replace {
    reg: reg_row1_value;

    update_lo_1_value: 1;
}

// action definition for sketch row 1 value replace
action act_row1_value_replace() {
    regact_row1_value_replace.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 value replace
@pragma stage 2
table tbl_row1_value_replace {
    actions {
        act_row1_value_replace;
    }
    default_action: act_row1_value_replace;
}



/*==================== Stable Bucket Array For Sketch Row 1 ====================*/
// register definition for sketch row 1 stability
register reg_row1_stable {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action for sketch row 1 stability update
blackbox stateful_alu regact_row1_stable_update {
    reg: reg_row1_stable;

    update_lo_1_value: register_lo + 1;
}

// action definition for sketch row 1 stability update
action act_row1_stable_update() {
    regact_row1_stable_update.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 stability update
@pragma stage 2
table tbl_row1_stable_update {
    actions {
        act_row1_stable_update;
    }
    default_action: act_row1_stable_update;
}

// register action for sketch row 1 stability read
blackbox stateful_alu regact_row1_stable_read {
    reg: reg_row1_stable;

    output_value: register_lo;
    output_dst: mdata.row1_stable;
}

// action definition for sketch row 1 stability read
action act_row1_stable_read() {
    regact_row1_stable_read.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 stability read
@pragma stage 2
table tbl_row1_stable_read {
    actions {
        act_row1_stable_read;
    }
    default_action: act_row1_stable_read;
}

// register action for sketch row 1 stability replace
blackbox stateful_alu regact_row1_stable_replace {
    reg: reg_row1_stable;

    condition_lo: register_lo == 0;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 0;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo - 1;
}

// action definition for sketch row 1 stability replace
action act_row1_stable_replace() {
    regact_row1_stable_replace.execute_stateful_alu(mdata.row1_loc);
}

// table definition for sketch row 1 stability replace
@pragma stage 2
table tbl_row1_stable_replace {
    actions {
        act_row1_stable_replace;
    }
    default_action: act_row1_stable_replace;
}



/*==================== Flowkey Bucket Array For Sketch Row 2 ====================*/
// register definition
register reg_row2_flow_key {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action for sketch row 2 flow key update
blackbox stateful_alu regact_row2_flow_key_update {
    reg: reg_row2_flow_key;

    condition_lo: register_lo == 0;
    condition_hi: register_hi == mdata.flow_key;

    update_lo_1_predicate: condition_lo or condition_hi;
    update_lo_1_value: register_lo + 1;
    // update_lo_2_predicate: not condition_lo and not condition_hi;
    // update_lo_2_value: register_lo;

    update_hi_1_predicate: condition_lo or condition_hi;
    update_hi_1_value: mdata.flow_key;
    // update_hi_2_predicate: not condition_lo and not condition_hi;
    // update_hi_2_value: register_hi;

    output_value: alu_hi;
    output_dst: mdata.row2_flow_key;

    initial_register_lo_value : 0;
    initial_register_hi_value : 0;
}

// action definition
action act_row2_flow_key_update() {
    regact_row2_flow_key_update.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 3
table tbl_row2_flow_key_update {
    actions {
        act_row2_flow_key_update;
    }
    default_action: act_row2_flow_key_update;
}


// register action for sketch row 2 flow key replace
blackbox stateful_alu regact_row2_flow_key_replace {
    reg: reg_row2_flow_key;

    update_hi_1_value: mdata.flow_key;
}

// action definition
action act_row2_flow_key_replace() {
    regact_row2_flow_key_replace.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 3
table tbl_row2_flow_key_replace {
    actions {
        act_row2_flow_key_replace;
    }
    default_action: act_row2_flow_key_replace;
}



/*==================== Value Bucket Array For Sketch Row 2 ====================*/
// register definition
register reg_row2_value {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action for sketch row 2 value update
blackbox stateful_alu regact_row2_value_update {
    reg: reg_row2_value;

    update_lo_1_value: register_lo + 1;
}

// action definition
action act_row2_value_update() {
    regact_row2_value_update.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 4
table tbl_row2_value_update {
    actions {
        act_row2_value_update;
    }
    default_action: act_row2_value_update;
}

// register action for sketch row 2 value read
blackbox stateful_alu regact_row2_value_read {
    reg: reg_row2_value;

    output_value: register_lo;
    output_dst: mdata.row2_value;
}

// action definition
action act_row2_value_read() {
    regact_row2_value_read.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 4
table tbl_row2_value_read {
    actions {
        act_row2_value_read;
    }
    default_action: act_row2_value_read;
}

// register action for sketch row 2 value replace
blackbox stateful_alu regact_row2_value_replace {
    reg: reg_row2_value;

    update_lo_1_value: 1;
}

// action definition
action act_row2_value_replace() {
    regact_row2_value_replace.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 4
table tbl_row2_value_replace {
    actions {
        act_row2_value_replace;
    }
    default_action: act_row2_value_replace;
}



/*==================== Stable Bucket Array For Sketch Row 2 ====================*/
// register definition
register reg_row2_stable {
    width: 32;
    instance_count: SKETCH_SIZE;
}

// register action for sketch row 2 stability update
blackbox stateful_alu regact_row2_stable_update {
    reg: reg_row2_stable;

    update_lo_1_value: register_lo + 1;
}

// action definition
action act_row2_stable_update() {
    regact_row2_stable_update.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 4
table tbl_row2_stable_update {
    actions {
        act_row2_stable_update;
    }
    default_action: act_row2_stable_update;
}

// register action for sketch row 2 stability read
blackbox stateful_alu regact_row2_stable_read {
    reg: reg_row2_stable;

    output_value: register_lo;
    output_dst: mdata.row2_stable;
}

// action definition
action act_row2_stable_read() {
    regact_row2_stable_read.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 4
table tbl_row2_stable_read {
    actions {
        act_row2_stable_read;
    }
    default_action: act_row2_stable_read;
}

// register action for sketch row 2 stability replace
blackbox stateful_alu regact_row2_stable_replace {
    reg: reg_row2_stable;

    condition_lo: register_lo == 0;

    update_lo_1_predicate: condition_lo;
    update_lo_1_value: 0;
    update_lo_2_predicate: not condition_lo;
    update_lo_2_value: register_lo - 1;
}

// action definition
action act_row2_stable_replace() {
    regact_row2_stable_replace.execute_stateful_alu(mdata.row2_loc);
}

// table definition
@pragma stage 4
table tbl_row2_stable_replace {
    actions {
        act_row2_stable_replace;
    }
    default_action: act_row2_stable_replace;
}



// =============================================================


/*==================== Flowkey Bucket Array For Sketch Row 3 ====================*/

// // register definition
// register reg_row3_flow_key {
//     width: 32;
//     instance_count: SKETCH_SIZE;
// }

// // register action for sketch row 3 flow key update
// blackbox stateful_alu regact_row3_flow_key_update {
//     reg: reg_row3_flow_key;

//     condition_lo: register_lo == 0;  // is this bucket empty?
//     condition_hi: register_hi == mdata.flow_key; // does this bucket store the flow?

//     update_lo_1_predicate: condition_lo or condition_hi; // it's empty or hit
//     update_lo_1_value: register_lo + 1; // note that any non-zero value indicates non-null
//     // update_lo_2_predicate: not condition_lo and not condition_hi;  // else read
//     // update_lo_2_value: register_lo;  // no change

//     update_hi_1_predicate: condition_lo or condition_hi;  // it's empty or hit
//     update_hi_1_value: mdata.flow_key;  // write the flow key
//     // update_hi_2_predicate: not condition_lo and not condition_hi;  // else read
//     // update_hi_2_value: register_hi;  // no change

//     output_value: alu_hi;  // read flow key to check whether update successfully
//     output_dst: mdata.row3_flow_key;

//     initial_register_lo_value : 0;
//     initial_register_hi_value : 0;
// }

// // action definition for sketch row 3 flow key update
// action act_row3_flow_key_update() {
//     regact_row3_flow_key_update.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 flow key update
// @pragma stage 3
// table tbl_row3_flow_key_update {
//     actions {
//         act_row3_flow_key_update;
//     }
//     default_action: act_row3_flow_key_update;
// }

// // register definition for sketch row 3 flow key update
// blackbox stateful_alu regact_row3_flow_key_replace {
//     reg: reg_row3_flow_key;

//     update_hi_1_value: mdata.flow_key;
// }

// // register action for sketch row 3 flow key replace
// action act_row3_flow_key_replace() {
//     regact_row3_flow_key_replace.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 flow key replace
// @pragma stage 3
// table tbl_row3_flow_key_replace {
//     actions {
//         act_row3_flow_key_replace;
//     }
//     default_action: act_row3_flow_key_replace;
// }



// /*==================== Value Bucket Array For Sketch Row 3 ====================*/

// // register definition for sketch row 3 value
// register reg_row3_value {
//     width: 32;
//     instance_count: SKETCH_SIZE;
// }

// // register action for sketch row 3 value update
// blackbox stateful_alu regact_row3_value_update {
//     reg: reg_row3_value;

//     update_lo_1_value: register_lo + 1;
// }

// // action definition for sketch row 3 value update
// action act_row3_value_update() {
//     regact_row3_value_update.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 value update
// @pragma stage 4
// table tbl_row3_value_update {
//     actions {
//         act_row3_value_update;
//     }
//     default_action: act_row3_value_update;
// }

// // register action for sketch row 3 value read
// blackbox stateful_alu regact_row3_value_read {
//     reg: reg_row3_value;

//     output_value: register_lo;
//     output_dst: mdata.row3_value;
// }

// // action definition for sketch row 3 value read
// action act_row3_value_read() {
//     regact_row3_value_read.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 value read
// @pragma stage 4
// table tbl_row3_value_read {
//     actions {
//         act_row3_value_read;
//     }
//     default_action: act_row3_value_read;
// }

// // register definition for sketch row 3 value replace
// blackbox stateful_alu regact_row3_value_replace {
//     reg: reg_row3_value;

//     update_lo_1_value: 1;
// }

// // action definition for sketch row 3 value replace
// action act_row3_value_replace() {
//     regact_row3_value_replace.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 value replace
// @pragma stage 4
// table tbl_row3_value_replace {
//     actions {
//         act_row3_value_replace;
//     }
//     default_action: act_row3_value_replace;
// }



// /*==================== Stable Bucket Array For Sketch Row 3 ====================*/

// // register definition for sketch row 3 stability
// register reg_row3_stable {
//     width: 32;
//     instance_count: SKETCH_SIZE;
// }

// // register action for sketch row 3 stability update
// blackbox stateful_alu regact_row3_stable_update {
//     reg: reg_row3_stable;

//     update_lo_1_value: register_lo + 1;
// }

// // action definition for sketch row 3 stability update
// action act_row3_stable_update() {
//     regact_row3_stable_update.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 stability update
// @pragma stage 4
// table tbl_row3_stable_update {
//     actions {
//         act_row3_stable_update;
//     }
//     default_action: act_row3_stable_update;
// }

// // register action for sketch row 3 stability read
// blackbox stateful_alu regact_row3_stable_read {
//     reg: reg_row3_stable;

//     output_value: register_lo;
//     output_dst: mdata.row3_stable;
// }

// // action definition for sketch row 3 stability read
// action act_row3_stable_read() {
//     regact_row3_stable_read.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 stability read
// @pragma stage 4
// table tbl_row3_stable_read {
//     actions {
//         act_row3_stable_read;
//     }
//     default_action: act_row3_stable_read;
// }

// // register action for sketch row 3 stability replace
// blackbox stateful_alu regact_row3_stable_replace {
//     reg: reg_row3_stable;

//     condition_lo: register_lo == 0;

//     update_lo_1_predicate: condition_lo;
//     update_lo_1_value: 0;
//     update_lo_2_predicate: not condition_lo;
//     update_lo_2_value: register_lo - 1;
// }

// // action definition for sketch row 3 stability replace
// action act_row3_stable_replace() {
//     regact_row3_stable_replace.execute_stateful_alu(mdata.row3_loc);
// }

// // table definition for sketch row 3 stability replace
// @pragma stage 4
// table tbl_row3_stable_replace {
//     actions {
//         act_row3_stable_replace;
//     }
//     default_action: act_row3_stable_replace;
// }