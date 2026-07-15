#pragma once
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <deque>
#include <map>
#include <climits>
#include <algorithm>
#include <cmath>
#include <array>
#include <fstream>
#include "bobhash.h"
#include "aion_bf.hpp"

// 任务ID：A(0)=包数(StableSketch)，B(1)=字节(StableSketch)，C(2)=持续性(P-Sketch)，D(3)=周期流(Aion)，E(4)=基数/子网基数(SegSketch)，F(5)=突发流(BurstSketch风格)
static constexpr int TASK_NUM = 6;

// SplitMix64：用于采样池/去重key混合（64-bit）
static inline uint64_t splitmix64(uint64_t x) {
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

class BloomFilter {
public:
    BloomFilter() = default;

    BloomFilter(const BloomFilter&) = delete;
    BloomFilter& operator=(const BloomFilter&) = delete;
    BloomFilter(BloomFilter&&) noexcept = default;
    BloomFilter& operator=(BloomFilter&&) noexcept = default;

    BloomFilter(uint32_t num_bits, int k_hash, uint32_t seed_base = 9000)
        : numBits(num_bits), k(k_hash) {
        if (numBits < 64) numBits = 64;
        bits.assign((numBits + 63) / 64, 0ULL);
        hashes.reserve(k);
        for (int i = 0; i < k; i++) {
            hashes.push_back(new BOBHash(seed_base + i * 101));
        }
    }

    ~BloomFilter() {
        for (auto* h : hashes) delete h;
        hashes.clear();
    }

    void clear() {
        std::fill(bits.begin(), bits.end(), 0ULL);
    }

    bool contains(uint64_t x) const {
        if (numBits == 0) return false;
        const char* p = reinterpret_cast<const char*>(&x);
        for (int i = 0; i < k; i++) {
            uint32_t hv = hashes[i]->run(p, sizeof(uint64_t));
            uint32_t pos = hv % numBits;
            uint64_t mask = 1ULL << (pos & 63);
            if ((bits[pos >> 6] & mask) == 0) return false;
        }
        return true;
    }

    void insert(uint64_t x) {
        if (numBits == 0) return;
        const char* p = reinterpret_cast<const char*>(&x);
        for (int i = 0; i < k; i++) {
            uint32_t hv = hashes[i]->run(p, sizeof(uint64_t));
            uint32_t pos = hv % numBits;
            bits[pos >> 6] |= 1ULL << (pos & 63);
        }
    }

    bool insertIfAbsent(uint64_t x) {
        if (contains(x)) return false;
        insert(x);
        return true;
    }

private:
    uint32_t numBits = 0;
    int k = 0;
    std::vector<uint64_t> bits;
    std::vector<BOBHash*> hashes;
};

class Sketch {
public:
    // ================= 共享桶/独占桶：统一 layout =================
    // - Exclusive：每个任务一份 sketch（桶里不需要 task id）
    // - Shared：六个任务共用一份桶池（桶里带 8-bit task id）
    // 每个桶：key + unit64 + unit64（shared 额外 + taskId）
    //
    // A/B (StableSketch): (u1=stability, u2=count)
    // C   (Pandora)     : (u1=inactivity|flag, u2=persistence_count)
    // D   (Aion)        : (u1,u2 合并为 128-bit count)
    // F   (Burst)       : (u1=pack(Cpre,Ccur), u2=timestamp)

    struct SharedBucket {
        uint64_t key;
        uint64_t u1;
        uint64_t u2;
        uint8_t task;
        SharedBucket() : key(UINT64_MAX), u1(0), u2(0), task(0) {}
    };

    struct ExBucket {
        uint64_t key;
        uint64_t u1;
        uint64_t u2;
        ExBucket() : key(UINT64_MAX), u1(0), u2(0) {}
    };

    struct FStage1Cell {
        uint64_t key;
        uint32_t cnt;
        FStage1Cell() : key(UINT64_MAX), cnt(0) {}
    };

    static constexpr uint64_t C_FLAG_MASK  = 1ULL << 63;
    static constexpr uint64_t C_INACT_MASK = (1ULL << 63) - 1ULL;
    static constexpr uint64_t C_REPLACE_Z  = 18ULL;
    static inline uint64_t c_inactivity(uint64_t u1) { return (u1 & C_INACT_MASK); }
    static inline bool c_flag(uint64_t u1) { return (u1 & C_FLAG_MASK) != 0; }
    static inline uint64_t c_pack_state(uint64_t inactivity, bool flag) {
        uint64_t x = (inactivity & C_INACT_MASK);
        if (flag) x |= C_FLAG_MASK;
        return x;
    }

    static constexpr uint32_t D_REPORT_THRESHOLD = 64;

    static constexpr uint32_t F_STAGE1_H = 6;
    static constexpr uint32_t F_BURST_T = 16;
    static constexpr uint32_t F_BURST_K = 2;
    static constexpr uint32_t F_BURST_L = 6;
    static constexpr uint32_t F_LAMBDA = 16;

    static inline uint32_t f_cpre(uint64_t u1) { return (uint32_t)(u1 >> 32); }
    static inline uint32_t f_ccur(uint64_t u1) { return (uint32_t)(u1 & 0xffffffffULL); }
    static inline uint64_t f_pack(uint32_t cpre, uint32_t ccur) { return ((uint64_t)cpre << 32) | (uint64_t)ccur; }
    static inline uint64_t f_scalar(uint64_t u1, uint64_t ts) {
        uint64_t v = std::max<uint64_t>((uint64_t)f_cpre(u1), (uint64_t)f_ccur(u1));
        if (ts != 0ULL) v = (UINT64_MAX - v < (uint64_t)F_LAMBDA) ? UINT64_MAX : (v + (uint64_t)F_LAMBDA);
        return v;
    }

    static inline unsigned __int128 aion_get128(uint64_t hi, uint64_t lo) {
        return ((unsigned __int128)hi << 64) | (unsigned __int128)lo;
    }
    static inline void aion_set128(unsigned __int128 v, uint64_t &hi, uint64_t &lo) {
        hi = (uint64_t)(v >> 64);
        lo = (uint64_t)v;
    }
    static inline unsigned __int128 aion_add128(unsigned __int128 a, unsigned __int128 b) {
        return a + b;
    }
    static inline uint64_t aion_scalar64(uint64_t hi, uint64_t lo) {
        if (hi != 0ULL) return UINT64_MAX;
        return lo;
    }

    // ===== SegSketch(E) 的 64-bit subnet/host bitmap =====
    // u1 = subnet bitmap, u2 = host bitmap
    static inline uint64_t seg_popcount64(uint64_t x) { return (uint64_t)__builtin_popcountll(x); }

    static inline uint64_t seg_est_card(uint64_t host_bm) {
        const double b = 64.0;
        uint64_t ones = seg_popcount64(host_bm);
        uint64_t z = (ones >= 64) ? 0 : (64 - ones);
        if (z == 0) z = 1;
        double est = b * std::log(b / (double)z);
        if (est < 0.0) est = 0.0;
        if (est > (double)UINT64_MAX) return UINT64_MAX;
        return (uint64_t)(est + 0.5);
    }

    static inline uint32_t seg_get_seg(uint32_t ip, int k) {
        int shift = 32 - 8 * k;
        return (shift >= 0) ? ((ip >> shift) & 0xFFu) : 0u;
    }

    static inline bool seg_range_has_one(uint64_t bm, int l, int r) {
        if (l < 0) l = 0;
        if (r > 64) r = 64;
        for (int i = l; i < r; ++i) {
            if (bm & (1ULL << i)) return true;
        }
        return false;
    }

    static inline uint32_t seg_halved_update(uint64_t &subnet_bm, uint32_t ip_dst, uint64_t seed = 0x534547ULL) {
        int left = 0, right = 63;
        int k = 1;
        const int V = 4;
        while (left < right && k < V) {
            int mid = (left + right) / 2;
            uint32_t seg = seg_get_seg(ip_dst, k);
            uint64_t h = splitmix64(((uint64_t)seg) ^ seed) & 1ULL;
            if (h == 1ULL) {
                if (seg_range_has_one(subnet_bm, left, mid)) {
                    subnet_bm |= (1ULL << mid);
                    return (uint32_t)k;
                }
                left = mid;
            } else {
                if (seg_range_has_one(subnet_bm, mid, right)) {
                    subnet_bm |= (1ULL << mid);
                    return (uint32_t)k;
                }
                right = mid;
            }
            k++;
        }
        subnet_bm |= (1ULL << left);
        return (uint32_t)k;
    }

    static inline void seg_host_update(uint64_t &host_bm, uint32_t ip_dst, uint32_t prefix_low_bits, uint64_t seed = 0x484F5354ULL) {
        uint32_t host_addr = ip_dst;
        if (prefix_low_bits >= 32) host_addr = 0u;
        else if (prefix_low_bits > 0) {
            uint32_t mask = (prefix_low_bits == 32) ? 0xFFFFFFFFu : ((1u << (32 - prefix_low_bits)) - 1u);
            host_addr = ip_dst & mask;
        }
        uint64_t pos = splitmix64(((uint64_t)host_addr) ^ seed) & 63ULL;
        host_bm |= (1ULL << pos);
    }

    struct KMVSampler {
        struct Entry { uint64_t h = 0; uint64_t v = 0; };
        size_t m = 0;
        uint64_t seed = 0;
        uint64_t tau = UINT64_MAX;
        std::unordered_map<uint64_t, Entry> mp;
        std::multimap<uint64_t, uint64_t> ord;
        bool enabled = true;
        KMVSampler() = default;
        KMVSampler(size_t m_, uint64_t seed_) { reset(m_, seed_); }
        void reset(size_t m_, uint64_t seed_);
        void clear();
        void observe(uint64_t key, uint64_t delta);
        void remove(uint64_t key);
        void dumpValues(std::vector<uint64_t>& out) const;
        uint64_t estimateDistinctN() const;
    };

    Sketch(int row, int col,
           uint32_t allTimeBFBits = (1u << 22),
           int bfK = 3,
           uint64_t skewIntervalWindows = 30,
           uint32_t skewSampleSize = 10000,
           uint32_t skewStableK = 5,
           double skewEps = 0.05);

    ~Sketch();

    void processPacket(int task, uint64_t pairKey, uint32_t pktSize, uint32_t srcIp, const std::string& hashKey);
    void endTimeWindow();

    std::unordered_map<uint64_t, uint64_t> getSharedMap(int task) const;
    std::unordered_map<uint64_t, uint64_t> getExMap(int task) const;

    const std::unordered_set<uint64_t>& getReportedDShared() const { return reportedDShared; }
    const std::unordered_set<uint64_t>& getReportedDExclusive() const { return reportedDExclusive; }
    const std::unordered_set<uint64_t>& getReportedFShared() const { return reportedFShared; }
    const std::unordered_set<uint64_t>& getReportedFExclusive() const { return reportedFExclusive; }

    struct BinaryMetrics {
        uint64_t tp = 0;
        uint64_t fp = 0;
        uint64_t fn = 0;
        double precision = 0.0;
        double recall = 0.0;
        double f1 = 0.0;
    };

    // 二分类评测接口：
    // - task 3(D) / 5(F): 只按 yes/no 统计 TP/FP/FN/Precision/Recall/F1，不统计值误差，也不统计 ARE。
    //   预测正例集合 = 当前 sketch 中仍保留的 key ∪ 已上报集合(reportedD*/reportedF*)。
    // - task 4(E): 若 positiveThreshold > 0，则把估计值/GT值 >= threshold 的 key 视为正例；
    //   若 CSV 已自带 label/gt/is_positive 列，也可直接按 yes/no 评测 Precision/Recall/F1。
    // - 若 CSV 没有 task 列，则视为“该 task 对应的一份专用 GT 文件”；
    //   若有 task 列，则只读取与 task 匹配的行。
    BinaryMetrics evaluateBinaryTaskAgainstCsv(int task, const std::string& csvPath, bool shared, uint64_t positiveThreshold = 0) const;
    std::unordered_set<uint64_t> getPredictedPositiveKeys(int task, bool shared, uint64_t positiveThreshold = 0) const;

private:
    int row = 0, col = 0, exCol = 0;

    // shared：六任务共享一份桶池
    SharedBucket **bucket = nullptr;

    // exclusive：每任务一份桶池
    ExBucket ***exBuckets = nullptr;

    // F 的 Stage1 在 shared / exclusive 中各自独立，不共享
    FStage1Cell **fStage1Shared = nullptr;
    FStage1Cell **fStage1Ex = nullptr;

    // 通用哈希（A/B/C/E 使用）；Aion(P) 使用其公开代码里的 BOBHash64(key, seed)
    BOBHash **hashFunctions = nullptr;

    static constexpr double ZIPF_ALPHA_INIT = 1.5;
    double alpha_cur[TASK_NUM];
    bool alpha_frozen[TASK_NUM];
    std::deque<double> alpha_hist[TASK_NUM];
    uint64_t skewIntervalWindows = 30;
    uint32_t skewSampleSize = 10000;
    uint32_t skewStableK = 5;
    double skewEps = 0.05;
    uint64_t skewWindowCnt = 0;
    KMVSampler samplers[TASK_NUM];

    uint64_t N_est[TASK_NUM];
    uint64_t M_mass[TASK_NUM];

    // A/B/C/D/E/F 六个任务的 M / 采样池 / N 都只在 shared 版本中维护；exclusive 版本不维护这些统计量。
    KMVSampler fSamplerShared;
    uint64_t fNEstShared = 0;
    uint64_t fMMassShared = 0;
    double fAlphaShared = ZIPF_ALPHA_INIT;
    bool fAlphaSharedFrozen = false;
    std::deque<double> fAlphaSharedHist;

    uint64_t curWindowId = 0;

    // C/D/E 三个任务分别使用 BloomFilter 去重：
    // C: 当前窗口内 distinct(src,dst) -> 对应总 distinct(s,d,t) 数（shared 统计）
    // D: 当前窗口内 distinct(src,dst) 仅用于 shared 侧的 M/N 与采样估计；Aion filter 与 sketch 插入仍对窗口内重复包继续执行
    // E: 全局 distinct(src,dst)      -> 对应总 distinct(s,d) 数（shared 统计）
    BloomFilter persistPairWinBF;
    BloomFilter aionPairWinBF;
    BloomFilter ePairBF;

    // D/F 共用黑名单 BloomFilter（shared/exclusive 分开维护，便于分别评测）
    BloomFilter blacklistDFShared;
    BloomFilter blacklistDFEx;

    std::unordered_set<uint64_t> reportedDShared;
    std::unordered_set<uint64_t> reportedDExclusive;
    std::unordered_set<uint64_t> reportedFShared;
    std::unordered_set<uint64_t> reportedFExclusive;

    // Aion Filter Part（shared / exclusive 分开维护）
    aion::BH_BF<uint64_t, uint64_t>* aionBHShared = nullptr;
    aion::BH_BF<uint64_t, uint64_t>* aionBHEx = nullptr;
    uint32_t aionTolerance = 17;
    uint64_t aionBfCounter = 1;

private:
    void onEndIntervalMaybeEstimate();
    static double estimateAlphaFromValues(const std::vector<uint64_t>& vals, double fallback);
    static double medianOfDeque(std::deque<double> dq);
    double getAlphaUsed(int task) const;
    double getAlphaUsedShared(int task) const;

    int getBucketIndex(int row_idx, const std::string& key, bool ex) const;

    double getPercentFast(int task, uint64_t value) const;
    double getPercentFastShared(int task, uint64_t value) const;
    double getPercentFast128(int task, unsigned __int128 value) const;
    uint64_t getValueFast(int task, double percent) const;
    uint64_t getValueFastShared(int task, double percent) const;
    unsigned __int128 getValueFast128(int task, double percent) const;

    uint64_t bucketScalar(int task, uint64_t u1, uint64_t u2) const;

    void processSharedStableAB(int task, uint64_t itemKey, uint32_t pktSize, const std::string& hashKey);
    void processSharedPSketchC(int task, uint64_t itemKey, const std::string& hashKey); // C task now follows Pandora logic
    void processSharedAionP(uint64_t itemKey, uint32_t dd);
    void processSharedSegSketchE(uint64_t hostKey, uint32_t ip_dst, const std::string& hostHashKey);
    void processSharedBurstF(uint64_t itemKey, const std::string& hashKey);
    bool processSharedBurstPromote(uint64_t itemKey, uint32_t C1, const std::string& hashKey);

    void processExclusiveStableAB(int task, uint64_t itemKey, uint32_t pktSize, const std::string& hashKey);
    void processExclusivePSketchC(int task, uint64_t itemKey, const std::string& hashKey); // C task now follows Pandora logic
    void processExclusiveAionP(uint64_t itemKey, uint32_t dd);
    void processExclusiveSegSketchE(uint64_t hostKey, uint32_t ip_dst, const std::string& hostHashKey);
    void processExclusiveBurstF(uint64_t itemKey, const std::string& hashKey);
    bool processExclusiveBurstPromote(uint64_t itemKey, uint32_t C1, const std::string& hashKey);

    void clearSharedBucket(SharedBucket &b);
    void clearExBucket(ExBucket &b);

    static bool hitProb(double p);
};