// get row1 percentile 
@pragma stage 0
table tbl_value_percentile_map_row1 {
    reads {
        mdata.row1_value:   exact;
        mdata.row1_task:    exact;
    }
    actions {
        act_value_percentile_map_row1;
    }
    size: 1024;
}

// get row2 percentile
@pragma stage 0
table tbl_value_percentile_map_row2 {
    reads {
        mdata.row2_value:   exact;
        mdata.row2_task:    exact;
    }
    actions {
        act_value_percentile_map_row2;
    }
    size: 1024;
}

// set row1 min 
@pragma stage 1
table tbl_set_row1_min {
    actions {
        act_set_row1_min;
    }
    default_action: act_set_row1_min;
}

// set row2 min 
@pragma stage 3
table tbl_set_row2_min {
    actions {
        act_set_row2_min;
    }
    default_action: act_set_row2_min;
}

// set minimum of row1 and row2
@pragma stage 2
table tbl_minimum_row1_row2 {
    actions {
        act_minimum_row1_row2;
    }
    default_action: act_minimum_row1_row2;
}

// min row percentile -> value map
@pragma stage 4
table tbl_min_percentile_value_map {
    reads {
        mdata.min_row_percentile: exact;
        user.task: exact;
    }
    actions {
        act_min_percentile_value_map;
    }
    size: 256;
}

// set probability
@pragma stage 5
table tbl_probability {
    reads {
        mdata.map_value: exact;
        user.delta: exact;
    }
    actions {
        act_probability;
    }
    size: 256;
}

// set random number
@pragma stage 5
table tbl_rand {
    actions {
        act_rand;
    }
    default_action: act_rand;
}

// whether to replace
@pragma stage 6
table tbl_is_replace {
    actions {
        act_is_replace;
    }
    default_action: act_is_replace;
}

// recirculate
@pragma stage 11
table tbl_recir {
    reads {
        mdata.is_replace: exact;
    }
    actions {
        act_recir;
        act_nop;
    }
    size: 2;
}


@pragma stage 7
table tbl_drop {
    reads {
        mdata.is_replace: exact;
    }
    actions {
        act_egress_drop;
        act_nop;
    }
    size: 2;
}

// calc frequency for AionSketch
@pragma stage 2
table tbl_report_frequency {
    reads {
        user.time_window: exact;
        mdata.aion_filter_sum: exact;
        mdata.aion_filter_count: exact;
    }
    actions {
        act_report_frequency;
    }
    size: 1024;
}

// calc period for AionSketch
@pragma stage 2
table tbl_report_period {
    reads {
        mdata.aion_filter_sum: exact;
        mdata.aion_filter_count: exact;
    }
    actions {
        act_report_period;
    }
    size: 1024;
}

// calc frequency for AionSketch
@pragma stage 2
table tbl_report_frequency_copy1 {
    reads {
        user.time_window: exact;
        mdata.aion_filter_sum: exact;
        mdata.aion_filter_count: exact;
    }
    actions {
        act_report_frequency;
    }
    size: 1024;
}

// calc period for AionSketch
@pragma stage 2
table tbl_report_period_copy1 {
    reads {
        mdata.aion_filter_sum: exact;
        mdata.aion_filter_count: exact;
    }
    actions {
        act_report_period;
    }
    size: 1024;
}