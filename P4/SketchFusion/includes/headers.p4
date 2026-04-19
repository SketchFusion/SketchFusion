/*==================== Header Definitions ====================*/
// Ethernet Header
header_type ethernet_t {
    fields {
        dstAddr : 48;
        srcAddr : 48;
        etherType : 16;
    }
}

// IPv4 Header
header_type ipv4_t {
    fields {
        version : 4;
        ihl : 4;
        diffserv : 8;
        totalLen : 16;
        identification : 16;
        flags : 3;
        fragOffset : 13;
        ttl : 8;
        protocol : 8;
        hdrChecksum : 16;
        srcAddr : 32;
        dstAddr: 32;
    }
}

// UDP Header
header_type udp_t {
    fields {
        srcPort:    16;
        dstPort:    16;
        totalLen:   16;
        checksum:   16;
    }
}

// User Custom Header
header_type user_t {
    fields {
        task:             8;
        time_window:      16;

        delta:            16;

        is_replace:       8;
    }
}



/*==================== Header Instances ====================*/
header ethernet_t ethernet;
header ipv4_t ipv4;
header udp_t udp;
header user_t user;



/*==================== Metadata Definition ====================*/
header_type metadata_t {
    fields {
        flow_key:           16;
        
        row1_loc:           16;
        row2_loc:           16;

        row1_flow_key:      16;
        row2_flow_key:      16;
        
        row1_task:          8;
        row2_task:          8;

        row1_value:         16;
        row2_value:         16;

        row1_percentile:    8;
        row2_percentile:    8;

        min_row_idx:        16;
        min_row_percentile: 8;
        min_task:           8;

        map_value:          16;

        m_probability:      8;
        m_rand:             8;

        is_replace:         8;

        aion_filter_loc:    16;
        burst_stage1_loc:   16;

        aion_filter_sum:    8;
        aion_filter_count:  8;
        burst_stage1_val:   8;

        aion_frequency:     16;
        aion_period:        16;
    }
}



/*==================== Metadata Instance ====================*/
// @pragma pa_container_size ingress ig_intr_md_for_tm.ucast_egress_port 16
// @pragma pa_container_size ingress ig_intr_md.ingress_port 16
metadata metadata_t mdata;