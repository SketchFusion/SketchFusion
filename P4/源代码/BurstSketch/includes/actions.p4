// nop action
action act_nop() {}

// action for logic time window get
action act_logic_time_window_get(logic_time_window_idx) {
    modify_field(mdata.logic_time_window_idx, logic_time_window_idx);
}

// action for drop
action act_drop() {
    modify_field(ig_intr_md_for_tm.drop_ctl, 1);
}

// action for recirculate
action act_recir() {
    modify_field(user.stage1_value, mdata.stage1_value);
    recirculate(68);
}

