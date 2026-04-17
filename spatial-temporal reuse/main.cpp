#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdint>
#include <string>
#include <cstdlib>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

#include "loader.h"
#include "sketch.h"
#include "ground.h"

// 将 uint32 转为 IPv4 字符串
static std::string ipToString(uint32_t ip_host) {
    struct in_addr ip_addr;
    uint32_t ip_network = htonl(ip_host);
    ip_addr.s_addr = ip_network;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, buf, INET_ADDRSTRLEN);
    return std::string(buf);
}

static inline std::string trim2(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) b++;
    while (e > b && std::isspace((unsigned char)s[e - 1])) e--;
    return s.substr(b, e - b);
}

static inline std::string stripQuotes2(const std::string& s) {
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

static inline std::vector<std::string> splitCSV2(const std::string& line) {
    std::vector<std::string> cols;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) cols.push_back(trim2(stripQuotes2(item)));
    return cols;
}

static inline std::string normalizeHeader2(std::string s) {
    std::string t;
    t.reserve(s.size());
    for (unsigned char ch : s) if (std::isalnum(ch)) t.push_back((char)std::tolower(ch));
    return t;
}

static inline bool isIPv4_2(const std::string& s) {
    struct in_addr addr;
    return ::inet_pton(AF_INET, s.c_str(), &addr) == 1;
}

struct ColumnIndex2 {
    int src = -1;
    int dst = -1;
    int task = -1;
    bool complete() const { return src >= 0 && dst >= 0 && task >= 0; }
};

static inline ColumnIndex2 tryParseHeader2(const std::vector<std::string>& cols) {
    ColumnIndex2 idx;
    std::unordered_map<std::string, int> pos;
    for (int i = 0; i < (int)cols.size(); ++i) pos[normalizeHeader2(cols[i])] = i;

    auto findAny = [&](std::initializer_list<const char*> names) -> int {
        for (const char* name : names) {
            auto it = pos.find(name);
            if (it != pos.end()) return it->second;
        }
        return -1;
    };

    idx.src = findAny({"sourceip", "srcip", "src"});
    idx.dst = findAny({"destip", "destinationip", "dstip", "dst"});
    idx.task = findAny({"taskid", "task", "label"});
    return idx;
}

static inline ColumnIndex2 defaultColumnIndex2(const std::vector<std::string>& cols) {
    ColumnIndex2 idx;
    if ((int)cols.size() >= 8) {
        idx.src = 0;
        idx.dst = 1;
        idx.task = 7;
    }
    return idx;
}

struct TaskFBinaryTruth {
    std::unordered_set<FlowKey, FlowKeyHash> universe;
    std::unordered_set<FlowKey, FlowKeyHash> positives;
};

static TaskFBinaryTruth loadTaskFBinaryTruth(const std::string& filename) {
    TaskFBinaryTruth data;
    std::ifstream file(filename);
    if (!file.is_open()) return data;

    std::string line;
    bool firstLineProcessed = false;
    ColumnIndex2 idx;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        auto cols = splitCSV2(line);
        if (cols.empty()) continue;

        if (!firstLineProcessed) {
            firstLineProcessed = true;
            ColumnIndex2 headerIdx = tryParseHeader2(cols);
            if (headerIdx.complete()) {
                idx = headerIdx;
                continue;
            }
            idx = defaultColumnIndex2(cols);
            if (!idx.complete()) break;
            if (!isIPv4_2(cols[idx.src]) || !isIPv4_2(cols[idx.dst])) break;
        }

        int maxNeeded = std::max({idx.src, idx.dst, idx.task});
        if ((int)cols.size() <= maxNeeded) continue;

        const std::string& src = cols[idx.src];
        const std::string& dst = cols[idx.dst];
        if (!isIPv4_2(src) || !isIPv4_2(dst)) continue;

        std::string task = cols[idx.task];
        if (task.empty()) continue;
        char label = (char)std::toupper((unsigned char)task[0]);
        if (label != 'F') continue;

        FlowKey fk(src, dst);
        data.universe.insert(fk);
        data.positives.insert(fk);
    }
    return data;
}

static int parseIntArg(int argc, char* argv[], int idx, int defaultVal) {
    return (argc > idx) ? std::atoi(argv[idx]) : defaultVal;
}

static uint64_t parseU64Arg(int argc, char* argv[], int idx, uint64_t defaultVal) {
    return (argc > idx) ? static_cast<uint64_t>(std::strtoull(argv[idx], nullptr, 10)) : defaultVal;
}

static uint32_t parseU32Arg(int argc, char* argv[], int idx, uint32_t defaultVal) {
    return (argc > idx) ? static_cast<uint32_t>(std::strtoul(argv[idx], nullptr, 10)) : defaultVal;
}

static double parseDoubleArg(int argc, char* argv[], int idx, double defaultVal) {
    return (argc > idx) ? std::strtod(argv[idx], nullptr) : defaultVal;
}

struct EvalResult {
    uint64_t tp = 0, fp = 0, fn = 0, tn = 0;
    double precision = 0.0, recall = 0.0, f1 = 0.0;
    double ARE = 0.0, AAE = 0.0;
    uint64_t eval_cnt = 0;
    bool has_value_error = true;
};

static EvalResult evalPairs(
    const std::unordered_map<uint64_t, uint64_t>& est,
    const std::unordered_map<FlowKey, uint64_t, FlowKeyHash>& truth,
    uint64_t threshold
) {
    EvalResult res;

    std::unordered_set<FlowKey, FlowKeyHash> predicted;
    predicted.reserve(est.size());

    double sumARE = 0.0;
    double sumAAE = 0.0;
    uint64_t cnt = 0;

    for (const auto& kv : est) {
        uint64_t key64 = kv.first;
        uint64_t v = kv.second;

        if (v <= threshold) continue;

        uint32_t src = static_cast<uint32_t>(key64 >> 32);
        uint32_t dst = static_cast<uint32_t>(key64 & 0xffffffffu);
        FlowKey fk(ipToString(src), ipToString(dst));
        predicted.insert(fk);

        auto it = truth.find(fk);
        uint64_t gt = (it == truth.end()) ? 0 : it->second;

        if (gt > threshold) res.tp++;
        else res.fp++;

        if (gt > 0) {
            sumARE += std::fabs(static_cast<double>(v) - static_cast<double>(gt)) / static_cast<double>(gt);
            sumAAE += std::fabs(static_cast<double>(v) - static_cast<double>(gt));
            cnt++;
        }
    }

    for (const auto& kv : truth) {
        const FlowKey& fk = kv.first;
        uint64_t gt = kv.second;
        bool isHeavy = (gt > threshold);
        bool isPred = (predicted.find(fk) != predicted.end());

        if (isHeavy && !isPred) res.fn++;
        if (!isHeavy && !isPred) res.tn++;
    }

    res.precision = (res.tp + res.fp) ? static_cast<double>(res.tp) / static_cast<double>(res.tp + res.fp) : 0.0;
    res.recall = (res.tp + res.fn) ? static_cast<double>(res.tp) / static_cast<double>(res.tp + res.fn) : 0.0;
    res.f1 = (res.precision + res.recall) ? (2.0 * res.precision * res.recall / (res.precision + res.recall)) : 0.0;

    res.eval_cnt = cnt;
    if (cnt > 0) {
        res.ARE = sumARE / static_cast<double>(cnt);
        res.AAE = sumAAE / static_cast<double>(cnt);
    }
    return res;
}

static EvalResult evalHosts(
    const std::unordered_map<uint64_t, uint64_t>& est,
    const std::unordered_map<uint32_t, uint64_t>& truth,
    uint64_t threshold
) {
    EvalResult res;

    std::unordered_set<uint32_t> predicted;
    predicted.reserve(est.size());

    double sumARE = 0.0;
    double sumAAE = 0.0;
    uint64_t cnt = 0;

    for (const auto& kv : est) {
        uint32_t src = static_cast<uint32_t>(kv.first);
        uint64_t v = kv.second;
        if (v <= threshold) continue;

        predicted.insert(src);

        auto it = truth.find(src);
        uint64_t gt = (it == truth.end()) ? 0 : it->second;

        if (gt > threshold) res.tp++;
        else res.fp++;

        if (gt > 0) {
            sumARE += std::fabs(static_cast<double>(v) - static_cast<double>(gt)) / static_cast<double>(gt);
        }
        sumAAE += std::fabs(static_cast<double>(v) - static_cast<double>(gt));
        cnt++;
    }

    for (const auto& kv : truth) {
        uint32_t src = kv.first;
        uint64_t gt = kv.second;
        bool isHeavy = (gt > threshold);
        bool isPred = (predicted.find(src) != predicted.end());

        if (isHeavy && !isPred) res.fn++;
        if (!isHeavy && !isPred) res.tn++;
    }

    res.precision = (res.tp + res.fp) ? static_cast<double>(res.tp) / static_cast<double>(res.tp + res.fp) : 0.0;
    res.recall    = (res.tp + res.fn) ? static_cast<double>(res.tp) / static_cast<double>(res.tp + res.fn) : 0.0;
    res.f1        = (res.precision + res.recall) ? (2.0 * res.precision * res.recall / (res.precision + res.recall)) : 0.0;
    res.eval_cnt  = cnt;

    if (cnt > 0) {
        res.ARE = sumARE / static_cast<double>(cnt);
        res.AAE = sumAAE / static_cast<double>(cnt);
    }
    return res;
}

static EvalResult evalBinaryPairsWithUniverse(
    const std::unordered_set<uint64_t>& predictedKeys,
    const std::unordered_set<FlowKey, FlowKeyHash>& truthPos,
    const std::unordered_set<FlowKey, FlowKeyHash>& universe
) {
    EvalResult res;
    res.has_value_error = false;

    std::unordered_set<FlowKey, FlowKeyHash> predicted;
    predicted.reserve(predictedKeys.size());

    for (uint64_t key64 : predictedKeys) {
        uint32_t src = (uint32_t)(key64 >> 32);
        uint32_t dst = (uint32_t)(key64 & 0xffffffffu);
        FlowKey fk(ipToString(src), ipToString(dst));
        if (universe.find(fk) == universe.end()) continue;
        predicted.insert(fk);
    }

    for (const auto& fk : universe) {
        bool isPos = (truthPos.find(fk) != truthPos.end());
        bool isPred = (predicted.find(fk) != predicted.end());
        if (isPos && isPred) res.tp++;
        else if (!isPos && isPred) res.fp++;
        else if (isPos && !isPred) res.fn++;
        else res.tn++;
    }

    res.precision = (res.tp + res.fp) ? (double)res.tp / (double)(res.tp + res.fp) : 0.0;
    res.recall = (res.tp + res.fn) ? (double)res.tp / (double)(res.tp + res.fn) : 0.0;
    res.f1 = (res.precision + res.recall) ? (2.0 * res.precision * res.recall / (res.precision + res.recall)) : 0.0;
    return res;
}

static void printRes(const std::string& title, const EvalResult& r) {
    std::cout << title << "\n";
    std::cout << "  TP=" << r.tp << " FP=" << r.fp << " FN=" << r.fn << " TN=" << r.tn << "\n";
    std::cout << "  Prec=" << r.precision << " Rec=" << r.recall << " F1=" << r.f1 << "\n";
    if (r.has_value_error) {
        std::cout << "  ARE=" << r.ARE << " AAE=" << r.AAE << " (cnt=" << r.eval_cnt << ")\n";
    }
}

template <typename MapT>
static uint64_t countPositives(const MapT& mp, uint64_t threshold) {
    uint64_t c = 0;
    for (const auto& kv : mp) {
        if (static_cast<uint64_t>(kv.second) > threshold) {
            ++c;
        }
    }
    return c;
}

template <typename MapT>
static std::unordered_set<uint64_t> keySetFromMap(const MapT& mp) {
    std::unordered_set<uint64_t> st;
    st.reserve(mp.size());
    for (const auto& kv : mp) st.insert(kv.first);
    return st;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr
            << "Usage: " << argv[0]
            << " <trace.pcap> <ground_truth_ABCD.csv>"
            << " [row=3] [col=1024] [window_us=1000]"
            << " [skew_interval_windows=30] [skew_sample_size=4096]"
            << " [skew_stable_k=5] [skew_eps=0.05]\n";
        return 1;
    }

    std::string dataFile = argv[1];
    std::string labelFile = argv[2];

    int row = parseIntArg(argc, argv, 3, 3);
    int col = parseIntArg(argc, argv, 4, 1024);

    // 一个时间窗长度改为 1ms = 1000us
    uint64_t windowUs = parseU64Arg(argc, argv, 5, 1000ULL);
    uint64_t skewIntervalWins = parseU64Arg(argc, argv, 6, 30ULL);
    uint32_t skewSampleSize = parseU32Arg(argc, argv, 7, 4096U);
    uint32_t skewStableK = parseU32Arg(argc, argv, 8, 5U);
    double skewEps = parseDoubleArg(argc, argv, 9, 0.05);

    GroundTruth labels;
    if (!labels.loadABCDLabels(labelFile)) {
        return 1;
    }
    TaskFBinaryTruth gtFTruth = loadTaskFBinaryTruth(labelFile);

    Loader loader(dataFile, 30000000000ULL);
    Sketch sketch(row, col, (1u << 22), 3, skewIntervalWins, skewSampleSize, skewStableK, skewEps);

    const auto& gtA_packets = labels.getGtA();
    const auto& gtB_bytes = labels.getGtB();
    const auto& gtC_persist = labels.getGtC();
    const auto& gtD_periodic = labels.getGtD();
    const auto& gtE_card = labels.getGtE();

    uint64_t totalA = labels.totalA();
    uint64_t totalB = labels.totalB();
    uint64_t totalC = labels.totalC();
    uint64_t totalD = labels.totalD();
    uint64_t totalE = labels.totalE();

    uint64_t thA = 60; //60
    uint64_t thB = 19000; //9000
    uint64_t thC = 45; //45
    uint64_t thD = 309;//309; //300
    uint64_t thE = 12;  //12

    std::cout << "Totals: A(packets)=" << totalA
              << " B(bytes)=" << totalB
              << " C(persist)=" << totalC
              << " D(periodic)=" << totalD
              << " E(card)=" << totalE << "\n";

    std::cout << "Thresholds(fixed): thA=" << thA
              << " thB=" << thB
              << " thC=" << thC
              << " thD=" << thD
              << " thE=" << thE << "\n";

    std::cout << "GT positives: "
              << " A=" << countPositives(gtA_packets, thA) << "/" << gtA_packets.size()
              << " B=" << countPositives(gtB_bytes, thB) << "/" << gtB_bytes.size()
              << " C=" << countPositives(gtC_persist, thC) << "/" << gtC_persist.size()
              << " D=" << countPositives(gtD_periodic, thD) << "/" << gtD_periodic.size()
              << " E=" << countPositives(gtE_card, thE) << "/" << gtE_card.size()
              << "\n";

    dataItem item;
    const double windowSec = (windowUs > 0) ? (static_cast<double>(windowUs) / 1e6) : 0.001; // 1ms 默认
    double firstTs = -1.0;
    uint64_t curWin = 0;

    while (loader.GetNext(&item) != -1) {
        uint64_t pairKey = item.item;
        uint32_t pktSize = item.packetsize;
        double ts = item.ts;

        // 即使该包不属于 ABCDE，也要推进时间窗
        if (firstTs < 0.0) {
            firstTs = ts;
            curWin = 0;
        }

        uint64_t newWin = 0;
        if (ts >= firstTs && windowSec > 0.0) {
            newWin = static_cast<uint64_t>(std::floor((ts - firstTs) / windowSec));
        }

        if (newWin > curWin) {
            for (uint64_t w = curWin; w < newWin; ++w) {
                sketch.endTimeWindow();
            }
            curWin = newWin;
        }

        uint32_t src = static_cast<uint32_t>(pairKey >> 32);
        uint32_t dst = static_cast<uint32_t>(pairKey & 0xffffffffu);

        std::string srcStr = ipToString(src);
        std::string dstStr = ipToString(dst);

        int task = labels.getJobABCD(srcStr, dstStr);
        if (task < 0) {
            continue;
        }

        //if( task==1 ||task==4)continue;
        std::string hashKey = srcStr + "|" + dstStr;
        sketch.processPacket(task, pairKey, pktSize, src, hashKey);
    }

    // 刷新最后一个时间窗
    sketch.endTimeWindow();

    auto shA = sketch.getSharedMap(0);
    auto shB = sketch.getSharedMap(1);
    auto shC = sketch.getSharedMap(2);
    auto shD = sketch.getSharedMap(3);
    auto shE = sketch.getSharedMap(4);
    auto shF = sketch.getSharedMap(5);
    std::cout<<"A冲突率"<<double(sketch.colA)/double(sketch.norA)<<" col:"<<sketch.colA<<" nor:"<<(sketch.norA)<<std::endl;
    std::cout<<"B冲突率"<<double(sketch.colB)/double(sketch.norB)<<" col:"<<sketch.colB<<" nor:"<<(sketch.norB)<<std::endl;
    std::cout<<"C冲突率"<<double(sketch.colC)/double(sketch.norC)<<" col:"<<sketch.colC<<" nor:"<<(sketch.norC)<<std::endl;
    std::cout<<"D冲突率"<<double(sketch.colD)/double(sketch.norD)<<" col:"<<sketch.colD<<" nor:"<<(sketch.norD)<<std::endl;
    std::cout<<"E冲突率"<<double(sketch.colE)/double(sketch.norE)<<" col:"<<sketch.colE<<" nor:"<<(sketch.norE)<<std::endl;
    std::cout<<"F冲突率"<<double(sketch.colF)/double(sketch.norF)<<" col:"<<sketch.colF<<" nor:"<<(sketch.norF)<<std::endl;

    std::cout<<"Sha A冲突率"<<double(sketch.shaA)/double(sketch.AllA)<<" sha:"<<sketch.shaA<<" ALLA"<<(sketch.AllA)<<std::endl;
    std::cout<<"Sha B冲突率"<<double(sketch.shaB)/double(sketch.AllB)<<" sha:"<<sketch.shaB<<" ALLB"<<(sketch.AllB)<<std::endl;
    std::cout<<"Sha C冲突率"<<double(sketch.shaC)/double(sketch.AllC)<<" sha:"<<sketch.shaC<<" ALLC"<<(sketch.AllC)<<std::endl;
    std::cout<<"Sha D冲突率"<<double(sketch.shaD)/double(sketch.AllD)<<" sha:"<<sketch.shaD<<" ALLD"<<(sketch.AllD)<<std::endl;
    std::cout<<"Sha E冲突率"<<double(sketch.shaE)/double(sketch.AllE)<<" sha:"<<sketch.shaE<<" ALLE"<<(sketch.AllE)<<std::endl;
    std::cout<<"Sha F冲突率"<<double(sketch.shaF)/double(sketch.AllF)<<" sha:"<<sketch.shaF<<" ALLF"<<(sketch.AllF)<<std::endl;


    std::cout<<"True Sha A冲突率"<<double(sketch.zhaA)/double(sketch.AllA)<<" zha:"<<sketch.zhaA<<" remainA:"<<sketch.inA+sketch.zhaA-sketch.endA<<std::endl;
    std::cout<<"True Sha B冲突率"<<double(sketch.zhaB)/double(sketch.AllB)<<" zha:"<<sketch.zhaB<<" remainB:"<<sketch.inB+sketch.zhaB-sketch.endB<<std::endl;
    std::cout<<"True Sha C冲突率"<<double(sketch.zhaC)/double(sketch.AllC)<<" zha:"<<sketch.zhaC<<" remainC:"<<sketch.inC+sketch.zhaC-sketch.endC<<std::endl;
    std::cout<<"True Sha D冲突率"<<double(sketch.zhaD)/double(sketch.AllD)<<" zha:"<<sketch.zhaD<<" remainD:"<<sketch.inD+sketch.zhaD-sketch.endD<<std::endl;
    std::cout<<"True Sha E冲突率"<<double(sketch.zhaE)/double(sketch.AllE)<<" zha:"<<sketch.zhaE<<" remainE:"<<sketch.inE+sketch.zhaE-sketch.endE<<std::endl;
    std::cout<<"True Sha F冲突率"<<double(sketch.zhaF)/double(sketch.AllF)<<" zha:"<<sketch.zhaF<<" remainF:"<<sketch.inF+sketch.zhaF-sketch.endF<<std::endl;

    std::cout<<"end A 突率"<<double(sketch.endA)/double(sketch.AllA)<<" end:"<<sketch.endA<<" in:"<<sketch.inA<<std::endl;
    std::cout<<"end B 突率"<<double(sketch.endB)/double(sketch.AllB)<<" end:"<<sketch.endB<<" in:"<<sketch.inB<<std::endl;
    std::cout<<"end C 突率"<<double(sketch.endC)/double(sketch.AllC)<<" end:"<<sketch.endC<<" in:"<<sketch.inC<<std::endl;
    std::cout<<"end D 突率"<<double(sketch.endD)/double(sketch.AllD)<<" end:"<<sketch.endD<<" in:"<<sketch.inD<<std::endl;
    std::cout<<"end E 突率"<<double(sketch.endE)/double(sketch.AllE)<<" end:"<<sketch.endE<<" in:"<<sketch.inE<<std::endl;
    std::cout<<"end F 突率"<<double(sketch.endF)/double(sketch.AllF)<<" end:"<<sketch.endF<<" in:"<<sketch.inF<<std::endl;


    auto exA = sketch.getExMap(0);
    auto exB = sketch.getExMap(1);
    auto exC = sketch.getExMap(2);
    auto exD = sketch.getExMap(3);
    auto exE = sketch.getExMap(4);
    auto exF = sketch.getExMap(5);

    printRes("[Shared] Task A (packets)", evalPairs(shA, gtA_packets, thA));
    printRes("[Exclusive] Task A (packets)", evalPairs(exA, gtA_packets, thA));

    printRes("[Shared] Task B (bytes)", evalPairs(shB, gtB_bytes, thB));
    printRes("[Exclusive] Task B (bytes)", evalPairs(exB, gtB_bytes, thB));

    printRes("[Shared] Task C (persistence)", evalPairs(shC, gtC_persist, thC));
    printRes("[Exclusive] Task C (persistence)", evalPairs(exC, gtC_persist, thC));

    printRes("[Shared] Task D (periodic)", evalPairs(shD, gtD_periodic, thD));
    printRes("[Exclusive] Task D (periodic)", evalPairs(exD, gtD_periodic, thD));

    printRes("[Shared] Task E (cardinality)", evalHosts(shE, gtE_card, thE));
    printRes("[Exclusive] Task E (cardinality)", evalHosts(exE, gtE_card, thE));

    auto predFShared = keySetFromMap(shF);
    auto predFExclusive = keySetFromMap(exF);
    printRes("[Shared] Task F (binary)", evalBinaryPairsWithUniverse(predFShared, gtFTruth.positives, gtFTruth.universe));
    printRes("[Exclusive] Task F (binary)", evalBinaryPairsWithUniverse(predFExclusive, gtFTruth.positives, gtFTruth.universe));

    return 0;
}