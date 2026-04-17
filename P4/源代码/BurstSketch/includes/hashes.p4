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

// filter part hash calculation
field_list_calculation filter_hash_calc {
    input {
        sketch_row_hash_fields;
    }
    algorithm : crc_16_dds_110;
    output_width : 16;
}

// sketch part row1 hash calculation
field_list_calculation stage1_hash_calc {
    input {
        sketch_row_hash_fields;
    }
    algorithm : crc_16_buypass;
    output_width : 16;
}

// sketch part row2 hash calculation 
field_list_calculation stage2_hash_calc {
    input {
        sketch_row_hash_fields;
    }
    algorithm : crc_16_dect;
    output_width: 16;
}

// sketch stage1 hash action
action act_stage1_bucket_hash() {
    modify_field_with_hash_based_offset(mdata.stage1_loc, 0, stage1_hash_calc, SKETCH_SIZE);
}

// sketch stage2 hash action
action act_stage2_bucket_hash() {
    modify_field_with_hash_based_offset(mdata.stage2_loc, 0, stage2_hash_calc, SKETCH_SIZE);
}

// sketch stage1 hash table
@pragma stage 0
table tbl_stage1_bucket_hash {
    actions {
        act_stage1_bucket_hash;
    }
    default_action: act_stage1_bucket_hash;
}

// sketch stage2 hash table
@pragma stage 0
table tbl_stage2_bucket_hash {
    actions {
        act_stage2_bucket_hash;
    }
    default_action: act_stage2_bucket_hash;
}