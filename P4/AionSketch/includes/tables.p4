// table definition for report period update
@pragma stage 2
table tbl_report_period_update {
    reads {
        mdata.cur_time_window: exact;
        mdata.filter_count:    exact;
        mdata.filter_sum:      exact;
    }
    actions {
        act_report_period_update;
        act_drop;
    }
    size: 65536;
}

// table definition for report frequency update
@pragma stage 3
table tbl_report_frequency_update {
    reads {
        mdata.report_period: exact;
        mdata.cur_time_window: exact;
    }
    actions {
        act_report_frequency_update;
        act_drop;
    }
    size: 65536;
}

// set sketch row1 min
@pragma stage 8
table tbl_set_min_row1 {
    actions {
        act_set_min_row1;
    }
    default_action: act_set_min_row1;
}

// get min of row1 and row2 
@pragma stage 9
table tbl_min_row1_row2 {
    actions {
        act_min_row1_row2;
    }
    default_action: act_min_row1_row2;
}

@pragma stage 10
table tbl_set_min_row2 {
    actions {
        act_set_min_row2;
    }
    default_action: act_set_min_row2;
}

// recirculate
@pragma stage 11
table tbl_recir {
    actions {
        act_recir;
    }
    default_action: act_recir;
}

// random number generation
@pragma stage 0
table tbl_rand {
    actions {
        act_rand;
    }
    default_action: act_rand;
}

// probability get
@pragma stage 0
table tbl_probability {
    actions {
        act_probability;
    }
    default_action: act_probability;
}

// whether can replace
@pragma stage 1
table tbl_is_replace {
    actions {
        act_is_replace;
    }
    default_action: act_is_replace;
}