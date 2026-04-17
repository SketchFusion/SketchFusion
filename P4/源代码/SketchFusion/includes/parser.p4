parser start {
    return parse_ethernet;
}

parser parse_ethernet {
    extract(ethernet);
    return select(latest.etherType) {
        0x0800 : parse_ipv4;
        default: ingress;
    }
}

parser parse_ipv4 {
    extract(ipv4);
    return select(ipv4.protocol) {
        17      :   parse_udp;
        default :   ingress;
    }
}

parser parse_udp {
    extract(udp);
    return parse_user;
}

parser parse_user {
    extract(user);
    return ingress;
}