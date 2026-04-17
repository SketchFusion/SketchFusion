#include "loader.h"

#include <iostream>
#include <cstring>
#include <vector>
#include <cstdio>

// =====================
//  PCAP (classic) 解析
//  - 不依赖 libpcap
//  - 仅抽取：ts、(src,dst)、length
//  - 只接受 IPv4，自动跳过 VLAN / 非 IPv4
// =====================

// classic pcap global header (24B)
struct PcapGlobalHdr {
    uint32_t magic;
    uint16_t ver_major;
    uint16_t ver_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network; // linktype
};

// classic pcap record header (16B)
struct PcapRecHdr {
    uint32_t ts_sec;
    uint32_t ts_sub;   // usec or nsec
    uint32_t incl_len;
    uint32_t orig_len;
};

static inline uint16_t bswap16(uint16_t x) {
    return (uint16_t)((x >> 8) | (x << 8));
}
static inline uint32_t bswap32(uint32_t x) {
    return ((x & 0x000000FFu) << 24) |
           ((x & 0x0000FF00u) << 8)  |
           ((x & 0x00FF0000u) >> 8)  |
           ((x & 0xFF000000u) >> 24);
}

static inline bool read_exact(FILE* f, void* buf, size_t n) {
    return std::fread(buf, 1, n, f) == n;
}

static inline uint16_t read_be16(const unsigned char* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    return ntohs(v);
}
static inline uint32_t read_be32(const unsigned char* p) {
    uint32_t v;
    std::memcpy(&v, p, sizeof(v));
    return ntohl(v);
}

// VLAN EtherTypes
static inline bool is_vlan_ethertype(uint16_t et) {
    return et == 0x8100 || et == 0x88A8 || et == 0x9100;
}

// 支持的常见 linktype
static constexpr uint32_t DLT_EN10MB     = 1;   // Ethernet
static constexpr uint32_t DLT_RAW        = 101; // Raw IP
static constexpr uint32_t DLT_LINUX_SLL  = 113; // Linux cooked capture (SLL)
static constexpr uint32_t DLT_LINUX_SLL2 = 276; // Linux cooked v2 (SLL2)
static constexpr uint32_t DLT_IPV4       = 228; // IPv4

// 从单个包里解析 IPv4 src/dst（host order）
static bool extract_ipv4_pair(
    const unsigned char* pkt,
    uint32_t caplen,
    uint32_t linktype,
    uint32_t& src_host,
    uint32_t& dst_host
) {
    uint32_t ipoff = 0;
    uint16_t ethertype = 0;

    if (linktype == DLT_EN10MB) {
        if (caplen < 14) return false;
        ethertype = read_be16(pkt + 12);
        uint32_t l2 = 14;
        while (is_vlan_ethertype(ethertype)) {
            // VLAN tag: 4 bytes (TCI + next EtherType)
            if (caplen < l2 + 4) return false;
            ethertype = read_be16(pkt + l2 + 2);
            l2 += 4;
        }
        if (ethertype != 0x0800) return false; // not IPv4
        ipoff = l2;
    } else if (linktype == DLT_LINUX_SLL) {
        // SLL header is 16 bytes, last 2 bytes are protocol
        if (caplen < 16) return false;
        uint16_t proto = read_be16(pkt + 14);
        if (proto != 0x0800) return false;
        ipoff = 16;
    } else if (linktype == DLT_LINUX_SLL2) {
        // SLL2 header is 20 bytes, first 2 bytes are protocol
        if (caplen < 20) return false;
        uint16_t proto = read_be16(pkt + 0);
        if (proto != 0x0800) return false;
        ipoff = 20;
    } else if (linktype == DLT_RAW || linktype == DLT_IPV4) {
        ipoff = 0;
    } else {
        // 其他 linktype 暂不支持
        return false;
    }

    if (caplen < ipoff + 20) return false;
    const unsigned char* ip = pkt + ipoff;
    uint8_t vihl = ip[0];
    uint8_t ver = (uint8_t)(vihl >> 4);
    uint8_t ihl = (uint8_t)(vihl & 0x0F);
    if (ver != 4 || ihl < 5) return false;
    uint32_t ihl_bytes = (uint32_t)ihl * 4;
    if (caplen < ipoff + ihl_bytes) return false;

    src_host = read_be32(ip + 12);
    dst_host = read_be32(ip + 16);
    return true;
}

// 兼容 Wireshark 的 "Length"：优先用 orig_len（线上长度），否则用 incl_len
static inline uint32_t pick_frame_len(uint32_t incl_len, uint32_t orig_len) {
    uint32_t v = (orig_len > 0) ? orig_len : incl_len;
    if (v == 0) v = incl_len;
    // 保守裁剪到合理范围（以免脏数据）
    const uint32_t MAX_REASONABLE = 65535u;
    if (v > MAX_REASONABLE) v = MAX_REASONABLE;
    return v;
}

Loader::Loader(std::string filename, uint64_t buffersize) {
    data = (loader*)calloc(1, sizeof(loader));
    data->databuffer = (unsigned char*)calloc(buffersize, sizeof(unsigned char));
    data->ptr = data->databuffer;
    data->cnt = 0;
    data->cur = 0;
    data->buf_size = buffersize;

    FILE* f = std::fopen(filename.c_str(), "rb");
    if (!f) {
        std::cerr << "[Error] Unable to open pcap file: " << filename << "\n";
        std::exit(-1);
    }

    PcapGlobalHdr gh{};
    if (!read_exact(f, &gh, sizeof(gh))) {
        std::cerr << "[Error] Failed to read pcap global header (file too small).\n";
        std::fclose(f);
        std::exit(-1);
    }

    // magic 判断端序/时间精度
    bool swapped = false;
    bool nano = false;
    if (gh.magic == 0xa1b2c3d4u) {
        swapped = false; nano = false;
    } else if (gh.magic == 0xd4c3b2a1u) {
        swapped = true; nano = false;
    } else if (gh.magic == 0xa1b23c4du) {
        swapped = false; nano = true;
    } else if (gh.magic == 0x4d3cb2a1u) {
        swapped = true; nano = true;
    } else {
        std::cerr << "[Error] Unsupported pcap magic: 0x" << std::hex << gh.magic << std::dec
                  << " (this loader supports classic pcap only; pcapng needs conversion).\n";
        std::fclose(f);
        std::exit(-1);
    }

    if (swapped) {
        gh.ver_major = bswap16(gh.ver_major);
        gh.ver_minor = bswap16(gh.ver_minor);
        gh.thiszone  = (int32_t)bswap32((uint32_t)gh.thiszone);
        gh.sigfigs   = bswap32(gh.sigfigs);
        gh.snaplen   = bswap32(gh.snaplen);
        gh.network   = bswap32(gh.network);
    }

    const uint32_t linktype = gh.network;

    unsigned char* p = data->databuffer;
    unsigned char* pend = data->databuffer + buffersize;

    uint64_t scanned = 0;
    uint64_t written = 0;
    uint64_t skipped_non_ipv4 = 0;
    uint64_t skipped_bad = 0;

    std::vector<unsigned char> pkt;
    pkt.reserve(2048);

    while (true) {
        PcapRecHdr rh{};
        if (!read_exact(f, &rh, sizeof(rh))) break;
        if (swapped) {
            rh.ts_sec   = bswap32(rh.ts_sec);
            rh.ts_sub   = bswap32(rh.ts_sub);
            rh.incl_len = bswap32(rh.incl_len);
            rh.orig_len = bswap32(rh.orig_len);
        }

        scanned++;
        if (rh.incl_len == 0) {
            skipped_bad++;
            continue;
        }

        pkt.resize(rh.incl_len);
        if (!read_exact(f, pkt.data(), rh.incl_len)) {
            // 文件截断
            break;
        }

        uint32_t src = 0, dst = 0;
        if (!extract_ipv4_pair(pkt.data(), rh.incl_len, linktype, src, dst)) {
            skipped_non_ipv4++;
            continue;
        }

        double ts = (double)rh.ts_sec + (double)rh.ts_sub / (nano ? 1e9 : 1e6);
        uint64_t item64 = ((uint64_t)src << 32) | (uint64_t)dst;
        uint32_t frame_len = pick_frame_len(rh.incl_len, rh.orig_len);

        const size_t need = sizeof(uint64_t) + sizeof(uint32_t) + sizeof(double);
        if (p + need > pend) {
            std::cerr << "[Warning] databuffer is full, stop reading early.\n";
            break;
        }

        std::memcpy(p, &item64, sizeof(item64));
        std::memcpy(p + sizeof(uint64_t), &frame_len, sizeof(frame_len));
        std::memcpy(p + sizeof(uint64_t) + sizeof(uint32_t), &ts, sizeof(ts));
        p += need;

        data->cnt++;
        written++;

        if (written <= 3) {
            std::cout << "[Debug] ts=" << ts
                      << " src=" << src
                      << " dst=" << dst
                      << " len=" << frame_len
                      << " (incl=" << rh.incl_len << ", orig=" << rh.orig_len << ")\n";
        }
    }

    std::fclose(f);
    std::cout << "[Message] PCAP parsed: scanned=" << scanned
              << ", kept_ipv4=" << written
              << ", skipped_non_ipv4=" << skipped_non_ipv4
              << ", skipped_bad=" << skipped_bad
              << ", linktype=" << linktype
              << ", ts_resolution=" << (nano ? "ns" : "us")
              << "\n";
}

Loader::~Loader() {
    free(data->databuffer);
    free(data);
}

int Loader::GetNext(dataItem* dataitem) {
    if (data->cur >= data->cnt) return -1;

    uint64_t item;
    uint32_t packetsize;
    double ts;
    std::memcpy(&item, data->ptr, sizeof(uint64_t));
    std::memcpy(&packetsize, data->ptr + sizeof(uint64_t), sizeof(uint32_t));
    std::memcpy(&ts, data->ptr + sizeof(uint64_t) + sizeof(uint32_t), sizeof(double));

    dataitem->item = item;
    dataitem->packetsize = packetsize;
    dataitem->ts = ts;

    data->cur++;
    data->ptr += sizeof(uint64_t) + sizeof(uint32_t) + sizeof(double);
    return 1;
}

void Loader::Reset() {
    data->cur = 0;
    data->ptr = data->databuffer;
}

uint64_t Loader::GetDataSize() { return data->cnt; }
uint64_t Loader::GetCurrent()  { return data->cur; }
