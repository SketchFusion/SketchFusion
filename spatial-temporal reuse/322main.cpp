#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdint>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

#include "loader.h"
#include "322sketch.h"
#include "322ground.h"

// Defined in 322sketch.cpp; no change to the original 322sketch.h is needed.
void setBlacklistFPRStartWindow(const Sketch* sketch, uint64_t startWindow);

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

static inline bool parseU64_2(const std::string& s, uint64_t& out) {
    char* endp = nullptr;
    unsigned long long v = std::strtoull(s.c_str(), &endp, 10);
    if (endp == s.c_str() || *endp != '\0') return false;
    out = (uint64_t)v;
    return true;
}

struct ColumnIndex2 {
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

static inline ColumnIndex2 defaultColumnIndex2(const std::vector<std::string>& cols) {
    ColumnIndex2 idx;
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

struct BinaryUniverse {
    std::unordered_set<FlowKey, FlowKeyHash> taskDAll;
    std::unordered_set<FlowKey, FlowKeyHash> taskFAll;
};

static BinaryUniverse loadBinaryTaskUniverse(const std::string& filename) {
    BinaryUniverse uni;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening label file for universe: " << filename << std::endl;
        return uni;
    }

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

        int maxNeeded = std::max({idx.src, idx.dst, idx.packet, idx.bytes, idx.persist, idx.periodic, idx.srcCard, idx.task});
        if ((int)cols.size() <= maxNeeded) continue;

        const std::string& src = cols[idx.src];
        const std::string& dst = cols[idx.dst];
        if (!isIPv4_2(src) || !isIPv4_2(dst)) continue;

        std::string task = cols[idx.task];
        if (task.empty()) continue;
        char label = (char)std::toupper((unsigned char)task[0]);
        FlowKey fk(src, dst);

        if (label == 'D') uni.taskDAll.insert(fk);
        else if (label == 'F') uni.taskFAll.insert(fk);
    }

    return uni;
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

        // 只在该任务 universe 内统计，避免跨任务键污染 FP/FN/TN
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


// Snapshot of the CURRENT sketch against the original full-trace CSV labels.
// This is not a per-interval F1, and warmup still contributes to predictions.
static void printF1Snapshot(
    const Sketch& sketch,
    const GroundTruth& labels,
    const BinaryUniverse& binaryUniverse,
    uint64_t finishedWindows
) {
    // Effective thresholds from the original final evaluation (strict >).
    constexpr uint64_t thA = 60;
    constexpr uint64_t thB = 19000;
    constexpr uint64_t thC = 45;
    constexpr uint64_t thE = 12;

    std::cout << "[F1 Snapshot] end_window=" << finishedWindows
              << " prediction_windows=1-" << finishedWindows
              << " label_scope=full_trace_csv\n";

    auto printOne = [&](const char* side, const char* task, const EvalResult& result) {
        const std::string title = "[F1] end_window=" + std::to_string(finishedWindows)
                                + " [" + side + "] Task " + task;
        printRes(title, result);
    };

    printOne("Shared", "A", evalPairs(sketch.getSharedMap(0), labels.getGtA(), thA));
    printOne("Exclusive", "A", evalPairs(sketch.getExMap(0), labels.getGtA(), thA));
    printOne("Shared", "B", evalPairs(sketch.getSharedMap(1), labels.getGtB(), thB));
    printOne("Exclusive", "B", evalPairs(sketch.getExMap(1), labels.getGtB(), thB));
    printOne("Shared", "C", evalPairs(sketch.getSharedMap(2), labels.getGtC(), thC));
    printOne("Exclusive", "C", evalPairs(sketch.getExMap(2), labels.getGtC(), thC));

    // D uses current bucket keys UNION all previously reported keys.
    auto predDShared = keySetFromMap(sketch.getSharedMap(3));
    mergeInto(predDShared, sketch.getReportedDShared());
    auto predDEx = keySetFromMap(sketch.getExMap(3));
    mergeInto(predDEx, sketch.getReportedDExclusive());
    printOne("Shared", "D (binary)",
             evalBinaryPairsWithUniverse(predDShared, labels.getGtDBinary(), binaryUniverse.taskDAll));
    printOne("Exclusive", "D (binary)",
             evalBinaryPairsWithUniverse(predDEx, labels.getGtDBinary(), binaryUniverse.taskDAll));

    printOne("Shared", "E", evalHosts(sketch.getSharedMap(4), labels.getGtE(), thE));
    printOne("Exclusive", "E", evalHosts(sketch.getExMap(4), labels.getGtE(), thE));

    // F uses reported keys; endTimeWindow() must run before this snapshot.
    printOne("Shared", "F (binary)",
             evalBinaryPairsWithUniverse(sketch.getReportedFShared(), labels.getGtFBinary(), binaryUniverse.taskFAll));
    printOne("Exclusive", "F (binary)",
             evalBinaryPairsWithUniverse(sketch.getReportedFExclusive(), labels.getGtFBinary(), binaryUniverse.taskFAll));
}

struct ApaeAccumulator {
    double sum_abs_err = 0.0;
    uint64_t count = 0;
};

static inline bool computeTruePercentileMidRank(
    const std::unordered_map<uint64_t, uint64_t>& truth,
    uint64_t key,
    double& outPct
) {
    auto it = truth.find(key);
    if (it == truth.end() || truth.empty()) return false;

    const uint64_t v = it->second;
    uint64_t greater = 0;
    uint64_t equal = 0;
    for (const auto& kv : truth) {
        if (kv.second > v) ++greater;
        else if (kv.second == v) ++equal;
    }
    if (equal == 0) return false;

    const double leftRank = (double)greater + 1.0;
    const double rightRank = (double)greater + (double)equal;
    const double rankMid = 0.5 * (leftRank + rightRank);
    outPct = ((double)truth.size() - rankMid + 1.0) / (double)truth.size();
    return true;
}

class IncrementalApaeTracker {
public:
    void onPacketObserved(int task, uint64_t pairKey, uint32_t src, uint32_t dst, uint32_t pktSize) {
        if (task == 0) {
            ++truthA[pairKey];
        } else if (task == 1) {
            truthB[pairKey] += (uint64_t)pktSize;
        } else if (task == 2) {
            if (seenCThisWindow.insert(pairKey).second) ++truthC[pairKey];
        } else if (task == 3) {
            if (seenDThisWindow.insert(pairKey).second) ++truthD[pairKey];
        } else if (task == 4) {
            const uint64_t hostKey = (uint64_t)src;
            auto &dstSet = truthEDistinctDst[hostKey];
            if (dstSet.insert(dst).second) ++truthE[hostKey];
        } else if (task == 5) {
            ++truthFThisWindow[pairKey];
        }
    }

    void onTimeWindowEnded() {
        seenCThisWindow.clear();
        seenDThisWindow.clear();
        truthFThisWindow.clear();
    }

    void consumeSketchEvents(Sketch& sketch) {
        auto events = sketch.drainApaeSampleEvents();
        for (const auto& ev : events) {
            if (ev.est_percentile < 0.0) continue;
            double truePct = 0.0;
            if (ev.task == 0) {
                if (!computeTruePercentileMidRank(truthA, ev.key, truePct)) continue;
                accA.sum_abs_err += std::fabs(ev.est_percentile - truePct);
                accA.count += 1;
            } else if (ev.task == 1) {
                if (!computeTruePercentileMidRank(truthB, ev.key, truePct)) continue;
                accB.sum_abs_err += std::fabs(ev.est_percentile - truePct);
                accB.count += 1;
            } else if (ev.task == 2) {
                if (!computeTruePercentileMidRank(truthC, ev.key, truePct)) continue;
                accC.sum_abs_err += std::fabs(ev.est_percentile - truePct);
                accC.count += 1;
            } else if (ev.task == 3) {
                if (!computeTruePercentileMidRank(truthD, ev.key, truePct)) continue;
                accD.sum_abs_err += std::fabs(ev.est_percentile - truePct);
                accD.count += 1;
            } else if (ev.task == 4) {
                if (!computeTruePercentileMidRank(truthE, ev.key, truePct)) continue;
                accE.sum_abs_err += std::fabs(ev.est_percentile - truePct);
                accE.count += 1;
            } else if (ev.task == 5) {
                if (!computeTruePercentileMidRank(truthFThisWindow, ev.key, truePct)) continue;
                accF.sum_abs_err += std::fabs(ev.est_percentile - truePct);
                accF.count += 1;
            }
        }
    }

    void printSummary() const {
        printOne("[APAE] Task A (incremental)", accA);
        printOne("[APAE] Task B (incremental)", accB);
        printOne("[APAE] Task C (incremental)", accC);
        printOne("[APAE] Task D (incremental)", accD);
        printOne("[APAE] Task E (incremental)", accE);
        printOne("[APAE] Task F (incremental)", accF);
    }

private:
    static void printOne(const std::string& title, const ApaeAccumulator& acc) {
        std::cout << title << "\n";
        if (acc.count == 0) {
            std::cout << "  APAE=N/A (cnt=0)\n";
            return;
        }
        std::cout << "  APAE=" << (acc.sum_abs_err / (double)acc.count)
                  << " (cnt=" << acc.count << ")\n";
    }

    std::unordered_map<uint64_t, uint64_t> truthA;
    std::unordered_map<uint64_t, uint64_t> truthB;
    std::unordered_map<uint64_t, uint64_t> truthC;
    std::unordered_map<uint64_t, uint64_t> truthD;
    std::unordered_map<uint64_t, uint64_t> truthE;
    std::unordered_map<uint64_t, uint64_t> truthFThisWindow;
    std::unordered_map<uint64_t, std::unordered_set<uint32_t>> truthEDistinctDst;
    std::unordered_set<uint64_t> seenCThisWindow;
    std::unordered_set<uint64_t> seenDThisWindow;

    ApaeAccumulator accA;
    ApaeAccumulator accB;
    ApaeAccumulator accC;
    ApaeAccumulator accD;
    ApaeAccumulator accE;
    ApaeAccumulator accF;
};

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

    // 窗口从 1 开始编号。例如起始 501、间隔 500，首次在第 1000 窗输出。
    constexpr uint64_t FPR_START_WINDOW = 100ULL;
    constexpr uint64_t FPR_REPORT_WINDOWS = 100ULL;
    constexpr uint32_t BF_BITS = 32768/4U; // 黑名单各用两倍位数，即各 8 KiB。
    constexpr int BF_HASHES = 3;
    static_assert(FPR_START_WINDOW >= 1 && FPR_REPORT_WINDOWS >= 1,
                  "FPR start window and report interval must be >= 1");

    GroundTruth labels;
    if (!labels.loadABCDLabels(labelFile)) return 1;
    BinaryUniverse binaryUniverse = loadBinaryTaskUniverse(labelFile);

    Loader loader(dataFile, 30000000000ULL);
    Sketch sketch(row, col, BF_BITS, BF_HASHES,
                  skewIntervalWins, skewSampleSize, skewStableK, skewEps);
    setBlacklistFPRStartWindow(&sketch, FPR_START_WINDOW);
    IncrementalApaeTracker apaeTracker;

    uint64_t finishedWindows = 0;
    uint64_t previousX[4] = {0, 0, 0, 0};
    uint64_t previousY[4] = {0, 0, 0, 0};

    std::cout << "[FPR/F1 Config] start_window=" << FPR_START_WINDOW
              << " report_windows=" << FPR_REPORT_WINDOWS << '\n';

    // Sketch.cpp starts sampling at FPR_START_WINDOW. Each never-inserted
    // (key, query_id) is sampled at most once FROM THAT WINDOW onward.
    // Counter differences cover new eligible samples in this report interval.
    auto printBFFPR = [&]() {
        const char* names[4] = {"F-Shared", "F-Exclusive", "D-Shared", "D-Exclusive"};
        const uint64_t x[4] = {
            static_cast<uint64_t>(sketch.FSC), static_cast<uint64_t>(sketch.FEC),
            static_cast<uint64_t>(sketch.DSC), static_cast<uint64_t>(sketch.DEC)
        };
        const uint64_t y[4] = {
            static_cast<uint64_t>(sketch.FSC1), static_cast<uint64_t>(sketch.FEC1),
            static_cast<uint64_t>(sketch.DSC1), static_cast<uint64_t>(sketch.DEC1)
        };
        auto printRate = [](uint64_t fp, uint64_t negatives) {
            if (negatives != 0) {
                std::cout << 100.0 * static_cast<double>(fp)
                                  / static_cast<double>(negatives) << "%";
            } else {
                std::cout << "N/A";
            }
        };
        for (int i = 0; i < 4; ++i) {
            const uint64_t dx = x[i] - previousX[i];
            const uint64_t dy = y[i] - previousY[i];
            std::cout << "[BF FPR] windows="
                      << (finishedWindows - FPR_REPORT_WINDOWS + 1)
                      << "-" << finishedWindows << " " << names[i]
                      << " interval_X=" << dx << " interval_Y=" << dy
                      << " interval_FPR=";
            printRate(dx, dy);
            std::cout << " cumulative_windows=" << FPR_START_WINDOW
                      << "-" << finishedWindows
                      << " cumulative_X=" << x[i] << " cumulative_Y=" << y[i]
                      << " cumulative_FPR=";
            printRate(x[i], y[i]);
            std::cout << '\n';
            previousX[i] = x[i];
            previousY[i] = y[i];
        }
    };

    // Use the same completion path for crossed windows and the last window.
    auto finishWindow = [&]() {
        sketch.endTimeWindow();
        apaeTracker.consumeSketchEvents(sketch);
        apaeTracker.onTimeWindowEnded();
        ++finishedWindows;
        if (finishedWindows >= FPR_START_WINDOW &&
            (finishedWindows - FPR_START_WINDOW + 1) % FPR_REPORT_WINDOWS == 0) {
            printBFFPR();
            printF1Snapshot(sketch, labels, binaryUniverse, finishedWindows);
        }
    };
 

    const char* TASK_NAME[TASK_NUM] = {"A", "B", "C", "D", "E", "F"};

    uint64_t totalInsertedPackets = 0;
    uint64_t taskPacketCnt[TASK_NUM] = {0, 0, 0, 0, 0, 0};
    std::unordered_set<uint64_t> taskFlowSet[TASK_NUM];

    // E 如果你还想顺手看“源主机数”，可以额外记这个
    std::unordered_set<uint32_t> taskEHostSet;

    double firstInsertedTs = -1.0;
    double lastInsertedTs = -1.0;
    dataItem item;
    const double windowSec = (windowUs > 0) ? ((double)windowUs / 1e6) : 1.0;
    double firstTs = -1.0;
    uint64_t curWin = 0;

    while (loader.GetNext(&item) != -1) {
        uint64_t pairKey = item.item;
        uint32_t pktSize = item.packetsize;
        double ts = item.ts;

        //if(ts>0.03&&ts<2.38)continue;

//        if(ts>2.4&&ts<3.4) continue;


  //      if(ts>3.5&&ts<3.6) continue;

    //    if(ts>3.9&&ts<4.0) continue;

      //  if(ts>4.4&&ts<4.666) continue;



        if (firstTs < 0.0) {
            firstTs = ts;
            curWin = 0;
        }
        uint64_t newWin = 0;
        if (ts >= firstTs && windowSec > 0.0) newWin = (uint64_t)std::floor((ts - firstTs) / windowSec);
        if (newWin > curWin) {
            for (uint64_t w = curWin; w < newWin; ++w) {
                finishWindow();
            }
            curWin = newWin;
        }

        uint32_t src = (uint32_t)(pairKey >> 32);
        uint32_t dst = (uint32_t)(pairKey & 0xffffffffu);
        std::string srcStr = ipToString(src);
        std::string dstStr = ipToString(dst);
        int task = labels.getJobABCD(srcStr, dstStr);
        if (task < 0) continue;
        

        //std::cout<<"ts: "<<ts<<" label:"<<task<<std::endl;
                // 统计“真正插入 sketch 的包”
        if (firstInsertedTs < 0.0) firstInsertedTs = ts;
        lastInsertedTs = ts;

        ++totalInsertedPackets;
        ++taskPacketCnt[task];
        taskFlowSet[task].insert(pairKey);
        if (task == 4) taskEHostSet.insert(src);  // 可选：E 的源主机数

        std::string hashKey = srcStr + "|" + dstStr;
        apaeTracker.onPacketObserved(task, pairKey, src, dst, pktSize);
        sketch.processPacket(task, pairKey, pktSize, src, hashKey);
        apaeTracker.consumeSketchEvents(sketch);
    }

    // As in the original program, EOF closes the last observed window.
    // Empty input does not create a window; a tail shorter than FPR_REPORT_WINDOWS
    // does not produce an extra FPR/F1 report.
    if (firstTs >= 0.0) finishWindow();

    double insertedDurationSec = 0.0;
    if (firstInsertedTs >= 0.0 && lastInsertedTs >= firstInsertedTs) {
        insertedDurationSec = lastInsertedTs - firstInsertedTs;
    }

    double totalPps = 0.0;
    if (insertedDurationSec > 0.0) {
        totalPps = (double)totalInsertedPackets / insertedDurationSec;
    }

    std::cout << "[Throughput]\n";
    std::cout << "  inserted_packets=" << totalInsertedPackets
              << " duration_sec=" << insertedDurationSec
              << " pps=" << totalPps << "\n";

    std::cout << "[Task Traffic Summary]\n";
    for (int t = 0; t < TASK_NUM; ++t) {
        double taskPps = 0.0;
        if (insertedDurationSec > 0.0) {
            taskPps = (double)taskPacketCnt[t] / insertedDurationSec;
        }

        std::cout << "  Task " << TASK_NAME[t]
                  << ": flows=" << taskFlowSet[t].size()
                  << " packets=" << taskPacketCnt[t]
                  << " pps=" << taskPps;

        if (t == 4) {
            std::cout << " hosts=" << taskEHostSet.size();
        }
        std::cout << "\n";
    }

    apaeTracker.printSummary();
    return 0;
}
