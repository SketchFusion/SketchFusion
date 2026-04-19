#define FLOW_KEY_HASH_SIZE 65536
#define SKETCH_SIZE 65536

/*==================== Flowkey Hash ====================*/
// flowkey hash fields
field_list flow_key_hash_fields {
    ipv4.srcAddr;
    ipv4.dstAddr;
    udp.srcPort;
    udp.dstPort;
    ipv4.protocol;
}

// flowkey hash calculation
field_list_calculation flow_key_hash_calc {
    input {
        flow_key_hash_fields;
    }
    algorithm : crc_16;
    output_width : 16;
}

// flowkey hash action
action act_flow_key_hash() {
    modify_field_with_hash_based_offset(mdata.flow_key, 0, flow_key_hash_calc, FLOW_KEY_HASH_SIZE);
}

// flowkey hash table
@pragma stage 0
table tbl_flow_key_hash {
    actions {
        act_flow_key_hash;
    }
    default_action: act_flow_key_hash;
}



/*==================== Sketch Row Hash ====================*/
// sketch row hash fields
field_list sketch_row_hash_fields {
    ipv4.srcAddr;
    ipv4.dstAddr;
    udp.srcPort;
    udp.dstPort;
    ipv4.protocol;
}

// sketch row 1 hash calculation
field_list_calculation sketch_row1_hash_calc {
    input {
        sketch_row_hash_fields;
    }
    algorithm : crc_16_buypass;
    output_width : 16;
}

// sketch row 2 hash calculation
field_list_calculation sketch_row2_hash_calc {
    input {
        sketch_row_hash_fields;
    }
    algorithm : crc_16_dds_110;
    output_width : 16;
}

// sketch row 3 hash calculation
// field_list_calculation sketch_row3_hash_calc {
//     input {
//         sketch_row_hash_fields;
//     }
//     algorithm : crc_16_dect;
//     output_width : 16;
// }

// sketch row 1 hash action
action act_sketch_row1_hash() {
    modify_field_with_hash_based_offset(mdata.row1_loc, 0, sketch_row1_hash_calc, SKETCH_SIZE);
}

// sketch row 2 hash action
action act_sketch_row2_hash() {
    modify_field_with_hash_based_offset(mdata.row2_loc, 0, sketch_row2_hash_calc, SKETCH_SIZE);
}

// sketch row 3 hash action
// action act_sketch_row3_hash() {
//     modify_field_with_hash_based_offset(mdata.row3_loc, 0, sketch_row3_hash_calc, SKETCH_SIZE);
// }

// sketch row 1 hash table 
@pragma stage 0
table tbl_sketch_row1_hash {
    actions {
        act_sketch_row1_hash;
    }
    default_action: act_sketch_row1_hash;
}

// sketch row 2 hash table
@pragma stage 0
table tbl_sketch_row2_hash {
    actions {
        act_sketch_row2_hash;
    }
    default_action: act_sketch_row2_hash;
}

// sketch row 3 hash table
// @pragma stage 0
// table tbl_sketch_row3_hash {
//     actions {
//         act_sketch_row3_hash;
//     }
//     default_action: act_sketch_row3_hash;
// }