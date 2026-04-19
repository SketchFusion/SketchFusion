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
        min_row_idx:    16;
        m_rand:         8;
        m_probability:  8;
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
        // row3_loc:           16;

        row1_flow_key:      16;
        row2_flow_key:      16;
        // row3_flow_key:      16; 

        row1_value:         16;
        row2_value:         16;
        // row3_value:         16;

        row1_stable:        16;
        row2_stable:        16;
        // row3_stable:        16;

        min_row_idx:        16;
        min_row_value:      16;
        min_row_stable:     16;

        m_rand:             8;
        m_probability:      8;

        can_replace:        8;
    }
}



/*==================== Metadata Instance ====================*/
metadata metadata_t mdata;