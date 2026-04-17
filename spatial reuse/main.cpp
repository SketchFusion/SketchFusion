#include <iostream>
#include <unordered_map>
#include <unordered_set>
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
    bool has_value_error = true;
};

static EvalResult evalPairs(
    const std::unordered_map<uint64_t, uint64_t>& est,
    const std::unordered_map<FlowKey, uint64_t, FlowKeyHash>& truth,
    uint64_t threshold
) {
    EvalResult res;
    res.has_value_error = true;
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
        //if(threshold==1)
        //std::cout<<"A"<<src<<" "<<dst<<std::endl;
        auto it = truth.find(fk);
        uint64_t gt = (it == truth.end()) ? 0 : it->second;
        if(threshold==1)
        std::cout<<"A"<<gt<<" "<<v<<std::endl;
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
    res.has_value_error = true;
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
        if (gt > 0) sumARE += std::fabs((double)v - (double)gt) / (double)gt;
        sumAAE += std::fabs((double)v - (double)gt);
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

static EvalResult evalBinaryPairs(
    const std::unordered_set<uint64_t>& predictedKeys,
    const std::unordered_set<FlowKey, FlowKeyHash>& truth
) {
    EvalResult res;
    res.has_value_error = false;
    std::unordered_set<FlowKey, FlowKeyHash> predicted;
    predicted.reserve(predictedKeys.size());

    for (uint64_t key64 : predictedKeys) {
        uint32_t src = (uint32_t)(key64 >> 32);
        uint32_t dst = (uint32_t)(key64 & 0xffffffffu);
        FlowKey fk(ipToString(src), ipToString(dst));
        predicted.insert(fk);
        if (truth.find(fk) != truth.end()) res.tp++;
        else res.fp++;
    }

    for (const auto& fk : truth) {
        if (predicted.find(fk) == predicted.end()) res.fn++;
    }

    res.precision = (res.tp + res.fp) ? (double)res.tp / (double)(res.tp + res.fp) : 0.0;
    res.recall = (res.tp + res.fn) ? (double)res.tp / (double)(res.tp + res.fn) : 0.0;
    res.f1 = (res.precision + res.recall) ? (2.0 * res.precision * res.recall / (res.precision + res.recall)) : 0.0;
    return res;
}

static std::unordered_set<uint64_t> keySetFromMap(const std::unordered_map<uint64_t, uint64_t>& mp) {
    std::unordered_set<uint64_t> s;
    s.reserve(mp.size());
    for (const auto& kv : mp) s.insert(kv.first);
    return s;
}

static void mergeInto(std::unordered_set<uint64_t>& dst, const std::unordered_set<uint64_t>& src) {
    dst.insert(src.begin(), src.end());
}

static void printRes(const std::string& title, const EvalResult& r) {
    std::cout << title << "\n";
    std::cout << "  TP=" << r.tp << " FP=" << r.fp << " FN=" << r.fn << " TN=" << r.tn << "\n";
    std::cout << "  Prec=" << r.precision << " Rec=" << r.recall << " F1=" << r.f1 << "\n";
    if (r.has_value_error) {
        std::cout << "  ARE=" << r.ARE << " AAE=" << r.AAE << " (cnt=" << r.eval_cnt << ")\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <trace.pcap> <ground_truth_ABCDEF.csv> [row=3] [col=1024] [window_us=1000] [skew_interval_windows=30] [skew_sample_size=4096] [skew_stable_k=5] [skew_eps=0.05]\n";
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
    //std::cerr<<"row:"<<row<<" col:"<<col<<std::endl;
    GroundTruth labels;
    if (!labels.loadABCDLabels(labelFile)) return 1;

    Loader loader(dataFile, 30000000000ULL);
    Sketch sketch(row, col, (1u << 22), 3, skewIntervalWins, skewSampleSize, skewStableK, skewEps);

    const auto& gtA_packets = labels.getGtA();
    const auto& gtB_bytes = labels.getGtB();
    const auto& gtC_persist = labels.getGtC();
    const auto& gtDBinary = labels.getGtDBinary();
    const auto& gtE_card = labels.getGtE();
    const auto& gtFBinary = labels.getGtFBinary();

    uint64_t totalA = labels.totalA();
    uint64_t totalB = labels.totalB();
    uint64_t totalC = labels.totalC();

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
        if (ts >= firstTs && windowSec > 0.0) newWin = (uint64_t)std::floor((ts - firstTs) / windowSec);
        if (newWin > curWin) {
            for (uint64_t w = curWin; w < newWin; ++w) sketch.endTimeWindow();
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

    const double ratio = 0.0006;
    const double ratioA = 0.01;
    uint64_t thA = (uint64_t)std::floor((double)totalA * ratioA);
    uint64_t thB = (uint64_t)std::floor((double)totalB * ratioA);
    uint64_t thC = (uint64_t)std::floor((double)totalC * ratio);
    uint64_t thE = 25;
    if (thA == 0) thA = 1;
    if (thB == 0) thB = 1;
    if (thC == 0) thC = 1;

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

    auto predDShared = keySetFromMap(shD);
    auto predDEx = keySetFromMap(exD);
    mergeInto(predDShared, sketch.getReportedDShared());
    mergeInto(predDEx, sketch.getReportedDExclusive());

    auto predFShared = sketch.getReportedFShared();
    auto predFEx = sketch.getReportedFExclusive();
    std::cout<<"taskA"<<shA.size()<<" "<<exA.size()<<" "<<gtA_packets.size()<<" "<<thA<<std::endl;
    std::cout<<"taskB"<<shB.size()<<" "<<exB.size()<<" "<<gtB_bytes.size()<<" "<<thB<<std::endl;
    std::cout<<"taskC"<<shC.size()<<" "<<exC.size()<<" "<<gtC_persist.size()<<" "<<thC<<std::endl;
    std::cout<<"taskD"<<shD.size()<<" "<<exD.size()<<std::endl;
    std::cout<<"taskE"<<shE.size()<<" "<<exE.size()<<" "<<thE<<std::endl;
    std::cout<<"taskF"<<predFShared.size()<<" "<<predFEx.size()<<std::endl;

    printRes("[Shared] Task A (packets)", evalPairs(shA, gtA_packets, thA));
    printRes("[Exclusive] Task A (packets)", evalPairs(exA, gtA_packets, thA));

    printRes("[Shared] Task B (bytes)", evalPairs(shB, gtB_bytes, thB));
    printRes("[Exclusive] Task B (bytes)", evalPairs(exB, gtB_bytes, thB));

    printRes("[Shared] Task C (persistence)", evalPairs(shC, gtC_persist, thC));
    printRes("[Exclusive] Task C (persistence)", evalPairs(exC, gtC_persist, thC));

    printRes("[Shared] Task D (periodic, binary)", evalBinaryPairs(predDShared, gtDBinary));
    printRes("[Exclusive] Task D (periodic, binary)", evalBinaryPairs(predDEx, gtDBinary));

    printRes("[Shared] Task E (cardinality)", evalHosts(shE, gtE_card, thE));
    printRes("[Exclusive] Task E (cardinality)", evalHosts(exE, gtE_card, thE));

    printRes("[Shared] Task F (burst, binary)", evalBinaryPairs(predFShared, gtFBinary));
    printRes("[Exclusive] Task F (burst, binary)", evalBinaryPairs(predFEx, gtFBinary));

    return 0;
}
