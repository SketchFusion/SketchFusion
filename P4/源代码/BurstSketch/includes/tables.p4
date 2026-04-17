// table for logic time window get
@pragma stage 1
table tbl_logic_time_window_get {
    reads {
        mdata.cur_time_window: ternary;
    }
    actions {
        act_logic_time_window_get;
        act_nop;
    }
    size: 1024;
}

// table for drop or recirculate
@pragma stage 9
table tbl_forward {
    reads {
        mdata.stage1_value: exact;
    }
    actions {
        act_drop;
        act_recir;
    }
    size: 3;
}