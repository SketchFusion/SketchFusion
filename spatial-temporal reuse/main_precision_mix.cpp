#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <arpa/inet.h>

#include "loader.h"
#include "sketch.h"
#include "ground.h"

static std::string ipToString(uint32_t ip_host) {
    struct in_addr ip_addr;
    uint32_t ip_network = htonl(ip_host);
    ip_addr.s_addr = ip_network;
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ip_addr, buf, INET_ADDRSTRLEN);
    return std::string(buf);
}

struct EvalResult {
    uint64_t tp = 0, fp = 0, fn = 0, tn = 0;
    double precision = 0.0, recall = 0.0, f1 = 0.0;
    double ARE = 0.0, AAE = 0.0;
    uint64_t eval_cnt = 0;
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

        uint32_t src = (uint32_t)(key64 >> 32);
        uint32_t dst = (uint32_t)(key64 & 0xffffffffu);
        FlowKey fk(ipToString(src), ipToString(dst));
        predicted.insert(fk);

        auto it = truth.find(fk);
        uint64_t gt = (it == truth.end()) ? 0 : it->second;
        if (gt > threshold) res.tp++;
        else res.fp++;

        if (gt > 0) {
            sumARE += std::fabs((double)v - (double)gt) / (double)gt;
            sumAAE += std::fabs((double)v - (double)gt);
            cnt++;
        }
    }

    for (const auto& kv : truth) {
        const FlowKey& fk = kv.first;
        uint64_t gt = kv.second;
        bool isHeavy = gt > threshold;
        bool isPred = (predicted.find(fk) != predicted.end());
        if (isHeavy && !isPred) res.fn++;
        if (!isHeavy && !isPred) res.tn++;
    }

    res.precision = (res.tp + res.fp) ? (double)res.tp / (double)(res.tp + res.fp) : 0.0;
    res.recall = (res.tp + res.fn) ? (double)res.tp / (double)(res.tp + res.fn) : 0.0;
    res.f1 = (res.precision + res.recall) ? (2.0 * res.precision * res.recall / (res.precision + res.recall)) : 0.0;
    res.eval_cnt = cnt;
    if (cnt > 0) {
        res.ARE = sumARE / (double)cnt;
        res.AAE = sumAAE / (double)cnt;
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
        uint32_t src = (uint32_t)kv.first;
        uint64_t v = kv.second;
        if (v <= threshold) continue;
        predicted.insert(src);

        auto it = truth.find(src);
        uint64_t gt = (it == truth.end()) ? 0 : it->second;
        if (gt > threshold) res.tp++;
        else res.fp++;

        if (gt > 0) {
            sumARE += std::fabs((double)v - (double)gt) / (double)gt;
        }
        sumAAE += std::fabs((double)v - (double)gt);
        cnt++;
    }

    for (const auto& kv : truth) {
        uint32_t src = kv.first;
        uint64_t gt = kv.second;
        bool isHeavy = gt > threshold;
        bool isPred = (predicted.find(src) != predicted.end());
        if (isHeavy && !isPred) res.fn++;
        if (!isHeavy && !isPred) res.tn++;
    }

    res.precision = (res.tp + res.fp) ? (double)res.tp / (double)(res.tp + res.fp) : 0.0;
    res.recall    = (res.tp + res.fn) ? (double)res.tp / (double)(res.tp + res.fn) : 0.0;
    res.f1        = (res.precision + res.recall) ? 2.0 * res.precision * res.recall / (res.precision + res.recall) : 0.0;
    res.eval_cnt  = cnt;
    if (cnt > 0) {
        res.ARE = sumARE / (double)cnt;
        res.AAE = sumAAE / (double)cnt;
    }
    return res;
}

static void printRes(const std::string& title, const EvalResult& r) {
    std::cout << title << "\n";
    std::cout << "  TP=" << r.tp << " FP=" << r.fp << " FN=" << r.fn << " TN=" << r.tn << "\n";
    std::cout << "  Prec=" << r.precision << " Rec=" << r.recall << " F1=" << r.f1 << "\n";
    std::cout << "  ARE=" << r.ARE << " AAE=" << r.AAE << " (cnt=" << r.eval_cnt << ")\n";
}

template <typename MapT>
static uint64_t countPositives(const MapT& mp, uint64_t threshold) {
    uint64_t c = 0;
    for (const auto& kv : mp) {
        if ((uint64_t)kv.second > threshold) ++c;
    }
    return c;
}

template <typename MapT>
static uint64_t autoThresholdFromTruth(const MapT& mp, double positiveRatio, uint64_t floorThreshold) {
    if (mp.empty()) return floorThreshold;
    std::vector<uint64_t> vals;
    vals.reserve(mp.size());
    for (const auto& kv : mp) {
        vals.push_back((uint64_t)kv.second);
    }
    std::sort(vals.begin(), vals.end(), std::greater<uint64_t>());

    size_t n = vals.size();
    size_t posCnt = (size_t)std::llround((double)n * positiveRatio);
    if (posCnt < 1) posCnt = 1;
    if (posCnt >= n) posCnt = n - 1;

    uint64_t posMin = vals[posCnt - 1];
    uint64_t negMax = vals[posCnt];

    uint64_t thr = floorThreshold;
    if (posMin > negMax) {
        thr = negMax;
    } else if (posMin > 0) {
        thr = posMin - 1;
    } else {
        thr = 0;
    }
    if (thr < floorThreshold) thr = floorThreshold;
    return thr;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <trace.pcap> <ground_truth.csv> [row=3] [col=1024] [window_us=1000] [skew_interval_windows=30] [skew_sample_size=4096] [skew_stable_k=5] [skew_eps=0.05]\n";
        return 1;
    }

    std::string dataFile = argv[1];
    std::string labelFile = argv[2];
    int row = (argc >= 4) ? std::stoi(argv[3]) : 3;
    int col = (argc >= 5) ? std::stoi(argv[4]) : 1024;
    uint64_t windowUs = (argc >= 6) ? (uint64_t)std::stoull(argv[5]) : 1000ULL;
    uint64_t skewIntervalWins = (argc >= 7) ? (uint64_t)std::stoull(argv[6]) : 30ULL;
    uint32_t skewSampleSize = (argc >= 8) ? (uint32_t)std::stoul(argv[7]) : 4096U;
    uint32_t skewStableK = (argc >= 9) ? (uint32_t)std::stoul(argv[8]) : 5U;
    double skewEps = (argc >= 10) ? std::stod(argv[9]) : 0.05;

    GroundTruth labels;
    if (!labels.loadABCDLabels(labelFile)) {
        return 1;
    }

    Loader loader(dataFile, 30000000000);
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

    dataItem item;
    const double windowSec = (windowUs > 0) ? ((double)windowUs / 1e6) : 1.0;
    double firstTs = -1.0;
    uint64_t curWin = 0;

    while (loader.GetNext(&item) != -1) {
        uint64_t pairKey = item.item;
        uint32_t pktSize = item.packetsize;
        double ts = item.ts;

        if (firstTs < 0.0) {
            firstTs = ts;
            curWin = 0;
        }
        uint64_t newWin = 0;
        if (ts >= firstTs && windowSec > 0.0) {
            newWin = (uint64_t)std::floor((ts - firstTs) / windowSec);
        }
        if (newWin > curWin) {
            for (uint64_t w = curWin; w < newWin; ++w) {
                sketch.endTimeWindow();
            }
            curWin = newWin;
        }

        uint32_t src = (uint32_t)(pairKey >> 32);
        uint32_t dst = (uint32_t)(pairKey & 0xffffffffu);
        std::string srcStr = ipToString(src);
        std::string dstStr = ipToString(dst);
        int task = labels.getJobABCD(srcStr, dstStr);
        if (task < 0) continue;
        std::string hashKey = srcStr + "|" + dstStr;
        sketch.processPacket(task, pairKey, pktSize, src, hashKey);
    }
    sketch.endTimeWindow();

    // 不再用固定魔数阈值，而是根据 ground truth 分布自动找 top30% / bottom70% 的分界。
    const double positiveRatio = 0.30;
    uint64_t thA = autoThresholdFromTruth(gtA_packets, positiveRatio, 8);
    uint64_t thB = autoThresholdFromTruth(gtB_bytes, positiveRatio, 4096);
    uint64_t thC = autoThresholdFromTruth(gtC_persist, positiveRatio, 6);
    uint64_t thD = autoThresholdFromTruth(gtD_periodic, positiveRatio, 4);
    uint64_t thE = autoThresholdFromTruth(gtE_card, positiveRatio, 12);

    std::cout << "Totals: A(packets)=" << totalA
              << " B(bytes)=" << totalB
              << " C(persist)=" << totalC
              << " D(periodic)=" << totalD
              << " E(card)=" << totalE << "\n";
    std::cout << "Thresholds(auto): thA=" << thA
              << " thB=" << thB
              << " thC=" << thC
              << " thD=" << thD
              << " thE=" << thE << "\n";
    std::cout << "GT positives: "
              << "A=" << countPositives(gtA_packets, thA) << "/" << gtA_packets.size()
              << " B=" << countPositives(gtB_bytes, thB) << "/" << gtB_bytes.size()
              << " C=" << countPositives(gtC_persist, thC) << "/" << gtC_persist.size()
              << " D=" << countPositives(gtD_periodic, thD) << "/" << gtD_periodic.size()
              << " E=" << countPositives(gtE_card, thE) << "/" << gtE_card.size()
              << "\n";

    auto shA = sketch.getSharedMap(0);
    auto shB = sketch.getSharedMap(1);
    auto shC = sketch.getSharedMap(2);
    auto shD = sketch.getSharedMap(3);
    auto shE = sketch.getSharedMap(4);
    auto exA = sketch.getExMap(0);
    auto exB = sketch.getExMap(1);
    auto exC = sketch.getExMap(2);
    auto exD = sketch.getExMap(3);
    auto exE = sketch.getExMap(4);

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
    return 0;
}
