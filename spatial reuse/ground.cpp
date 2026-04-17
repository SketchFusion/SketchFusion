#include "322ground.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <unordered_map>
#include <arpa/inet.h>

static inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) b++;
    while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}

static inline std::string stripQuotes(const std::string& s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static inline std::vector<std::string> splitCSV(const std::string& line) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) cols.push_back(trim(stripQuotes(item)));
    return cols;
}

static inline bool isIPv4(const std::string& s) {
    struct in_addr addr;
    return ::inet_pton(AF_INET, s.c_str(), &addr) == 1;
}

static inline bool isIP(const std::string& s) {
    struct in_addr addr4;
    if (::inet_pton(AF_INET, s.c_str(), &addr4) == 1) return true;
    struct in6_addr addr6;
    return ::inet_pton(AF_INET6, s.c_str(), &addr6) == 1;
}

static inline bool parseABCDLabel(const std::string& token, char& out) {
    if (token.empty()) return false;
    char c = (char)std::toupper((unsigned char)token[0]);
    if (c >= 'A' && c <= 'F') {
        out = c;
        return true;
    }
    return false;
}

static inline bool parseU64(const std::string& s, uint64_t& out) {
    char* endp = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &endp, 10);
    if (endp == s.c_str() || *endp != '\0') return false;
    out = (uint64_t)v;
    return true;
}

static inline std::string normalizeHeader(std::string s) {
    std::string t;
    t.reserve(s.size());
    for (unsigned char ch : s) if (std::isalnum(ch)) t.push_back((char)std::tolower(ch));
    return t;
}

struct ColumnIndex {
    int src = -1;
    int dst = -1;
    int packet = -1;
    int bytes = -1;
    int persist = -1;
    int periodic = -1;
    int srcCard = -1;
    int task = -1;

    bool complete() const {
        return src >= 0 && dst >= 0 && packet >= 0 && bytes >= 0 &&
               persist >= 0 && periodic >= 0 && srcCard >= 0 && task >= 0;
    }
};

static inline ColumnIndex tryParseHeader(const std::vector<std::string>& cols) {
    ColumnIndex idx;
    std::unordered_map<std::string, int> pos;
    for (int i = 0; i < (int)cols.size(); ++i) pos[normalizeHeader(cols[i])] = i;

    auto findAny = [&](std::initializer_list<const char*> names) -> int {
        for (const char* name : names) {
            auto it = pos.find(name);
            if (it != pos.end()) return it->second;
        }
        return -1;
    };

    idx.src      = findAny({"sourceip", "srcip", "src"});
    idx.dst      = findAny({"destip", "destinationip", "dstip", "dst"});
    idx.packet   = findAny({"packetcount", "packets", "pktcount", "pktcnt"});
    idx.bytes    = findAny({"bytecount", "bytes", "bytecnt"});
    idx.persist  = findAny({"persistence", "persist"});
    idx.periodic = findAny({"periodicity", "periodic", "period"});
    idx.srcCard  = findAny({"srccardinality", "sourcecardinality", "cardinality", "srccard"});
    idx.task     = findAny({"taskid", "task", "label"});
    return idx;
}

static inline ColumnIndex defaultColumnIndex(const std::vector<std::string>& cols) {
    ColumnIndex idx;
    if ((int)cols.size() >= 8) {
        idx.src = 0;
        idx.dst = 1;
        idx.packet = 2;
        idx.bytes = 3;
        idx.persist = 4;
        idx.periodic = 5;
        idx.srcCard = 6;
        idx.task = 7;
    }
    return idx;
}

bool GroundTruth::loadABCDLabels(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening label file: " << filename << std::endl;
        return false;
    }

    flowLabels.clear();
    gtA_packets.clear();
    gtB_bytes.clear();
    gtC_persist.clear();
    gtD_periodic.clear();
    gtE_card.clear();
    gtD_binary.clear();
    gtF_binary.clear();
    totalA_packets = totalB_bytes = totalC_persist = totalD_periodic = totalE_card = 0;

    std::string line;
    bool firstLineProcessed = false;
    ColumnIndex idx;
    size_t cnt = 0;
    size_t cntA = 0, cntB = 0, cntC = 0, cntD = 0, cntE = 0, cntF = 0;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto cols = splitCSV(line);
        if (cols.empty()) continue;

        if (!firstLineProcessed) {
            firstLineProcessed = true;
            ColumnIndex headerIdx = tryParseHeader(cols);
            if (headerIdx.complete()) {
                idx = headerIdx;
                std::cout << "Detected CSV header mapping in " << filename
                          << ": src=" << idx.src
                          << " dst=" << idx.dst
                          << " packet=" << idx.packet
                          << " bytes=" << idx.bytes
                          << " persistence=" << idx.persist
                          << " periodicity=" << idx.periodic
                          << " srcCard=" << idx.srcCard
                          << " task=" << idx.task << std::endl;
                continue;
            }

            idx = defaultColumnIndex(cols);
            if (!idx.complete()) {
                std::cerr << "CSV format error in " << filename << ": need at least 8 usable columns." << std::endl;
                return false;
            }
            if (!isIP(cols[idx.src]) || !isIP(cols[idx.dst])) {
                std::cerr << "Unexpected CSV header/order in " << filename << std::endl;
                return false;
            }
        }

        int maxNeeded = std::max({idx.src, idx.dst, idx.packet, idx.bytes, idx.persist, idx.periodic, idx.srcCard, idx.task});
        if ((int)cols.size() <= maxNeeded) continue;

        const std::string& src = cols[idx.src];
        const std::string& dst = cols[idx.dst];
        if (!isIPv4(src) || !isIPv4(dst)) continue;

        char label = 0;
        if (!parseABCDLabel(cols[idx.task], label)) continue;

        uint64_t packetCnt = 0, byteCnt = 0, persistCnt = 0, periodicCnt = 0, srcCard = 0;
        parseU64(cols[idx.packet], packetCnt);
        parseU64(cols[idx.bytes], byteCnt);
        parseU64(cols[idx.persist], persistCnt);
        parseU64(cols[idx.periodic], periodicCnt);
        parseU64(cols[idx.srcCard], srcCard);

        FlowKey fk(src, dst);
        flowLabels[fk] = label;

        if (label == 'A') {
            gtA_packets[fk] = packetCnt;
            totalA_packets += packetCnt;
            cntA++;
        } else if (label == 'B') {
            gtB_bytes[fk] = byteCnt;
            totalB_bytes += byteCnt;
            cntB++;
        } else if (label == 'C') {
            gtC_persist[fk] = persistCnt;
            totalC_persist += persistCnt;
            cntC++;
        } else if (label == 'D') {
            gtD_periodic[fk] = periodicCnt;
            gtD_binary.insert(fk);
            totalD_periodic += periodicCnt;
            cntD++;
        } else if (label == 'E') {
            struct in_addr addr;
            if (::inet_pton(AF_INET, src.c_str(), &addr) == 1) {
                uint32_t src32 = ntohl(addr.s_addr);
                auto it = gtE_card.find(src32);
                if (it == gtE_card.end()) {
                    gtE_card[src32] = srcCard;
                    totalE_card += srcCard;
                } else if (srcCard > it->second) {
                    totalE_card += (srcCard - it->second);
                    it->second = srcCard;
                }
            }
            cntE++;
        } else if (label == 'F') {
            gtF_binary.insert(fk);
            cntF++;
        }
        cnt++;
    }

    std::cout << "Loaded labels: " << cnt
              << " (A=" << cntA
              << ", B=" << cntB
              << ", C=" << cntC
              << ", D=" << cntD
              << ", E=" << cntE
              << ", F=" << cntF << ")" << std::endl;
    return true;
}

char GroundTruth::getLabel(const std::string& src_ip, const std::string& dst_ip) const {
    auto it = flowLabels.find(FlowKey(src_ip, dst_ip));
    if (it == flowLabels.end()) return 0;
    return it->second;
}

int GroundTruth::getJobABCD(const std::string& src_ip, const std::string& dst_ip) const {
    char label = getLabel(src_ip, dst_ip);
    if (label == 'A') return 0;
    if (label == 'B') return 1;
    if (label == 'C') return 2;
    if (label == 'D') return 3;
    if (label == 'G') return 3;
    if (label == 'E') return 4;
    if (label == 'F') return 5;
    return -1;
}
