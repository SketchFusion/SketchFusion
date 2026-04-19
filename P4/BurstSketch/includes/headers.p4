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
    fields{
        stage1_value: 8;
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
        flow_key:               16;
        
        stage1_loc:             16;
        stage2_loc:             16;

        cur_time_window:        16;
        is_window_updated:      16;
        logic_time_window_idx:  8;

        cell1_flow_key:         16;
        cell2_flow_key:         16;

        stage1_flow_key:        16;
        stage1_value:           8; 

        cell1_cur_value:        16;
        cell2_cur_value:        16;

        m_rand:                 8;
        m_probability:          8;

        can_replace:            8;
    }
}



/*==================== Metadata Instance ====================*/
@pragma pa_container_size ingress ig_intr_md_for_tm.ucast_egress_port 16
@pragma pa_container_size ingress ig_intr_md.ingress_port 16
metadata metadata_t mdata;