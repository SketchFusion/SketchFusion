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
#include "bobhash.h"
#include "aion_bf.hpp"

// 任务ID：A(0)=包数(StableSketch)，B(1)=字节(StableSketch)，C(2)=持续性(P-Sketch)，P(3)=周期流(Aion)，E(4)=基数/子网基数(SegSketch)，F(5)=突发流(BurstSketch风格)
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
    // P   (Aion)        : (u1,u2 合并为 128-bit count)

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
struct FStage1Cell {
        uint64_t key;
        uint32_t cnt;
        FStage1Cell() : key(UINT64_MAX), cnt(0) {}
    };



    static constexpr uint32_t D_REPORT_THRESHOLD = 1;//64;

    // 与 build_6task_prefix_truncate.py 和 compute_ground_truth_abcdef_prefix_truncate.py 对齐：
    // H=2, T=4, K=2, L=8，window_us 仍由 main 默认 1000us 控制。
    static constexpr uint32_t F_STAGE1_H = 2;
    static constexpr uint32_t F_BURST_T = 4;
    static constexpr uint32_t F_BURST_K = 2;
    static constexpr uint32_t F_BURST_L = 8;
    static constexpr uint32_t F_LAMBDA = 16;
    static constexpr uint64_t F_CONFIRMED_MASK = 1ULL << 63;
    static constexpr uint64_t F_TS_MASK = F_CONFIRMED_MASK - 1ULL;

    KMVSampler fSamplerShared;
    uint64_t fNEstShared = 0;
    uint64_t fMMassShared = 0;
    double fAlphaShared = ZIPF_ALPHA_INIT;
    bool fAlphaSharedFrozen = false;
    std::deque<double> fAlphaSharedHist;
    static inline uint32_t f_cpre(uint64_t u1) { return (uint32_t)(u1 >> 32); }
    static inline uint32_t f_ccur(uint64_t u1) { return (uint32_t)(u1 & 0xffffffffULL); }
    static inline uint64_t f_pack(uint32_t cpre, uint32_t ccur) { return ((uint64_t)cpre << 32) | (uint64_t)ccur; }
    static inline uint64_t f_ts(uint64_t u2) { return (u2 & F_TS_MASK); }
    static inline bool f_confirmed(uint64_t u2) { return (u2 & F_CONFIRMED_MASK) != 0ULL; }
    static inline uint64_t f_pack_meta(uint64_t ts, bool confirmed) {
        uint64_t x = (ts & F_TS_MASK);
        if (confirmed) x |= F_CONFIRMED_MASK;
        return x;
    }
    static inline uint64_t f_scalar(uint64_t u1, uint64_t u2) {
        if (f_confirmed(u2)) return UINT64_MAX;
        uint64_t v = (uint64_t)f_ccur(u1);
        if (f_ts(u2) != 0ULL) v = (UINT64_MAX - v < (uint64_t)F_LAMBDA) ? UINT64_MAX : (v + (uint64_t)F_LAMBDA);
        return v;
    }
    Sketch(int row, int col,
           uint32_t allTimeBFBits = (1u << 22),
           int bfK = 3,
           uint64_t skewIntervalWindows = 30,
           uint32_t skewSampleSize = 10000,
           uint32_t skewStableK = 5,
           double skewEps = 0.05);

    ~Sketch();

    int norA=0;
    int norB=0;
    int norC=0;
    int norD=0;
    int norE=0;
    int norF=0;

    int colA=0;
    int colB=0;
    int colC=0;
    int colD=0;
    int colE=0;
    int colF=0;

    int shaA=0;
    int shaB=0;
    int shaC=0;
    int shaD=0;
    int shaE=0;
    int shaF=0;
int zhaA=0;
    int zhaB=0;
    int zhaC=0;
    int zhaD=0;
    int zhaE=0;
    int zhaF=0;
    int AllA=0;
    int AllB=0;
    int AllC=0;
    int AllD=0;
    int AllE=0;
    int AllF=0;

    int endA=0;
    int endB=0;
    int endC=0;
    int endD=0;
    int endE=0;
    int endF=0;

    int inA=0;
    int inB=0;
    int inC=0;
    int inD=0;
    int inE=0;
    int inF=0;
    void processPacket(int task, uint64_t pairKey, uint32_t pktSize, uint32_t srcIp, const std::string& hashKey);
    void endTimeWindow();
 void processExclusiveBurstF(uint64_t itemKey, const std::string& hashKey);
    bool processExclusiveBurstPromote(uint64_t itemKey, uint32_t C1, const std::string& hashKey);

    std::unordered_map<uint64_t, uint64_t> getSharedMap(int task) const;
    std::unordered_map<uint64_t, uint64_t> getExMap(int task) const;

private:
    int row = 0, col = 0, exCol = 0;

    // shared：五任务共享一份桶池
    SharedBucket **bucket = nullptr;

    // exclusive：每任务一份桶池
    ExBucket ***exBuckets = nullptr;
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

    uint64_t curWindowId = 0;

    // C/D/E 三个任务分别使用 BloomFilter 去重：
    // C: 当前窗口内 distinct(src,dst) -> 对应总 distinct(s,d,t) 数
    // D: 当前窗口内 distinct(src,dst) 仅用于 M/N 与采样估计；Aion filter 与 sketch 插入仍对窗口内重复包继续执行
    // E: 全局 distinct(src,dst)      -> 对应总 distinct(s,d) 数
    BloomFilter persistPairWinBF;
    BloomFilter aionPairWinBF;
    BloomFilter ePairBF;
    BloomFilter blacklistDFShared;
    BloomFilter blacklistDFEx;

    // Aion Filter Part
    aion::BH_BF<uint64_t, uint64_t>* aionBH = nullptr;
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

    unsigned __int128 getValueFast128(int task, double percent) const;

    uint64_t getValueFast(int task, double percent) const;
    uint64_t getValueFastShared(int task, double percent) const;
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

    static bool hitProb(double p);
    static void clearSharedBucket(SharedBucket &b);
    static void clearExBucket(ExBucket &b);
};