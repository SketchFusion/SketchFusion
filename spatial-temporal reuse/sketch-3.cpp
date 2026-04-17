#include "sketch.h"
#include <iostream>
#include <random>
#include <cmath>
int dol=0;
int dod=0;
int doe=0;
int del=0;


// hhh===================== 通用小工具 =====================
static inline double clip01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}
static inline double clipd(double x, double lo, double hi) {
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}
static inline uint64_t clipu64(uint64_t x, uint64_t lo, uint64_t hi) {
    return std::min<uint64_t>(std::max<uint64_t>(x, lo), hi);
}
static inline uint64_t percentile_rank_k(double percent, uint64_t N) {
    if (N == 0) return 0;
    long double p = (long double)clip01(percent);
    long double raw_k = ceill(p * (long double)N);
    uint64_t k = (raw_k <= 0.0L) ? 0 : (uint64_t)raw_k;
    return clipu64(k, 1, N);
}
static inline uint64_t sat_add_u64(uint64_t a, uint64_t b) {
    unsigned __int128 s = (unsigned __int128)a + (unsigned __int128)b;
    return (s > (unsigned __int128)UINT64_MAX) ? UINT64_MAX : (uint64_t)s;
}

// ===================== Zipf 百分位互推（按 PPT 公式） =====================
static inline double H_hat_zipf(uint64_t N, double alpha) {
    if (N == 0) return 1.0;
    double Nd = (double)N;
    return (std::pow(Nd, 1.0 - alpha) - 1.0) / (1.0 - alpha)
         + (1.0 + std::pow(Nd, -alpha)) / 2.0;
}
static inline double C_hat_zipf(uint64_t M, uint64_t N, double alpha) {
    double H = H_hat_zipf(N, alpha);
    if (H <= 0.0) return 0.0;
    return (double)M / H;
}

// ===================== 随机源 =====================
static thread_local std::mt19937_64 g_rng(1234567);
static thread_local std::uniform_real_distribution<double> g_uni(0.0, 1.0);

bool Sketch::hitProb(double p) {
    if (p <= 0.0) return false;
    if (p >= 1.0) return true;
    return g_uni(g_rng) < p;
}

void Sketch::clearSharedBucket(SharedBucket &b) {
    b.key = UINT64_MAX;
    b.u1 = 0ULL;
    b.u2 = 0ULL;
    b.task = 0;
}

void Sketch::clearExBucket(ExBucket &b) {
    b.key = UINT64_MAX;
    b.u1 = 0ULL;
    b.u2 = 0ULL;
}

// ===================== KMVSampler（bottom-m 哈希采样） =====================
void Sketch::KMVSampler::reset(size_t m_, uint64_t seed_) {
    m = std::max<size_t>(m_, 16);
    seed = seed_;
    clear();
    enabled = true;
}

void Sketch::KMVSampler::clear() {
    mp.clear();
    ord.clear();
    tau = UINT64_MAX;
}

void Sketch::KMVSampler::dumpValues(std::vector<uint64_t>& out) const {
    out.clear();
    out.reserve(mp.size());
    for (const auto& kv : mp) out.push_back(kv.second.v);
}

uint64_t Sketch::KMVSampler::estimateDistinctN() const {
    if (mp.empty()) return 0ULL;
    if (mp.size() < m || ord.empty()) return (uint64_t)mp.size();

    long double um = ((long double)tau + 1.0L) / 18446744073709551616.0L; // 2^64
    if (!(um > 0.0L)) return (uint64_t)mp.size();

    long double est = ((long double)m - 1.0L) / um;
    if (est < (long double)mp.size()) est = (long double)mp.size();
    if (est > (long double)UINT64_MAX) return UINT64_MAX;
    return (uint64_t)(est + 0.5L);
}

void Sketch::KMVSampler::observe(uint64_t key, uint64_t delta) {
    if (!enabled) return;
    if (delta == 0) return;

    auto it = mp.find(key);
    if (it != mp.end()) {
        it->second.v = sat_add_u64(it->second.v, delta);
        return;
    }

    uint64_t h = splitmix64(key ^ seed);

    if (mp.size() < m) {
        mp[key] = Entry{h, delta};
        ord.insert({h, key});
        tau = ord.empty() ? UINT64_MAX : ord.rbegin()->first;
        return;
    }

    if (h >= tau) return;

    mp[key] = Entry{h, delta};
    ord.insert({h, key});

    auto itMax = std::prev(ord.end());
    uint64_t kk = itMax->second;
    ord.erase(itMax);
    mp.erase(kk);

    tau = ord.empty() ? UINT64_MAX : ord.rbegin()->first;
}

void Sketch::KMVSampler::remove(uint64_t key) {
    auto it = mp.find(key);
    if (it == mp.end()) return;
    uint64_t h = it->second.h;
    auto range = ord.equal_range(h);
    for (auto oit = range.first; oit != range.second; ++oit) {
        if (oit->second == key) {
            ord.erase(oit);
            break;
        }
    }
    mp.erase(it);
    tau = ord.empty() ? UINT64_MAX : ord.rbegin()->first;
}

// ===================== alpha(skewness) 估计与冻结逻辑 =====================
static inline double clampAlpha(double a) {
    if (!std::isfinite(a)) return 1.8;
    if (a < 1) return 1;
    if (a > 1.8) return 1.8;
    return a;
}

// 用 tail-Hill 估计 Zipf 的 alpha
double Sketch::estimateAlphaFromValues(const std::vector<uint64_t>& vals, double fallback) {
    std::vector<double> v;
    v.reserve(vals.size());
    for (auto x : vals) if (x > 0) v.push_back((double)x);
    if (v.size() < 16) return fallback;

    std::sort(v.begin(), v.end(), std::greater<double>());

    const size_t K0 = 100;
    size_t K = std::min(K0, v.size());
    if (K < 10) return fallback;

    double xmin = v[K - 1];
    if (xmin <= 0.0) return fallback;

    double sum = 0.0;
    for (size_t i = 0; i + 1 < K; i++) sum += std::log(std::max(1e-12, v[i] / xmin));
    if (!(sum > 1e-12)) return fallback;

    double a_hat = (double)(K - 1) / sum;
    if (!(a_hat > 1e-12)) return fallback;

    double s_hat = 1.0 / a_hat;
    s_hat = clampAlpha(s_hat);
    if (!(s_hat > 0.0)) return fallback;
    return s_hat;
}

double Sketch::medianOfDeque(std::deque<double> dq) {
    if (dq.empty()) return Sketch::ZIPF_ALPHA_INIT;
    std::vector<double> v(dq.begin(), dq.end());
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    if (n & 1) return v[n/2];
    return 0.5 * (v[n/2 - 1] + v[n/2]);
}

double Sketch::getAlphaUsed(int task) const {
    if (task < 0 || task >= TASK_NUM) return ZIPF_ALPHA_INIT;
    if (alpha_frozen[task]) return alpha_cur[task];
    if (alpha_hist[task].empty()) return alpha_cur[task];
    double s = 0.0;
    for (double a : alpha_hist[task]) s += a;
    return s / (double)alpha_hist[task].size();
}

void Sketch::onEndIntervalMaybeEstimate() {
    if (skewIntervalWindows == 0) return;
    if (skewWindowCnt % skewIntervalWindows != 0) return;

    for (int t = 0; t < TASK_NUM; t++) {
        if (t == 5) continue;
        N_est[t] = samplers[t].estimateDistinctN();
        if (alpha_frozen[t]) continue;

        std::vector<uint64_t> vals;
        samplers[t].dumpValues(vals);

        double est = estimateAlphaFromValues(vals, alpha_cur[t]);
        alpha_cur[t] = est;

        alpha_hist[t].push_back(est);
        while (alpha_hist[t].size() > skewStableK) alpha_hist[t].pop_front();

        if (alpha_hist[t].size() == skewStableK && skewStableK >= 2) {
            double mn = alpha_hist[t][0], mx = alpha_hist[t][0];
            for (double a : alpha_hist[t]) { mn = std::min(mn, a); mx = std::max(mx, a); }
            if ((mx - mn) <= skewEps) {
                double stable = medianOfDeque(alpha_hist[t]);
                alpha_cur[t] = stable;
                alpha_frozen[t] = true;
            }
        }
    }

    fNEstShared = fSamplerShared.estimateDistinctN();
    N_est[5] = fNEstShared;
    if (!fAlphaSharedFrozen) {
        std::vector<uint64_t> vals;
        fSamplerShared.dumpValues(vals);
        double est = estimateAlphaFromValues(vals, fAlphaShared);
        fAlphaShared = est;
        fAlphaSharedHist.push_back(est);
        while (fAlphaSharedHist.size() > skewStableK) fAlphaSharedHist.pop_front();
        if (fAlphaSharedHist.size() == skewStableK && skewStableK >= 2) {
            double mn = fAlphaSharedHist[0], mx = fAlphaSharedHist[0];
            for (double a : fAlphaSharedHist) { mn = std::min(mn, a); mx = std::max(mx, a); }
            if ((mx - mn) <= skewEps) {
                double stable = medianOfDeque(fAlphaSharedHist);
                fAlphaShared = stable;
                fAlphaSharedFrozen = true;
            }
        }
    }
}

// ===================== 构造/析构 =====================
Sketch::Sketch(int row_, int col_, uint32_t allTimeBFBits, int bfK,
               uint64_t skewIntervalWindows_, uint32_t skewSampleSize_, uint32_t skewStableK_, double skewEps_)
    : row(row_), col(col_) {
    skewIntervalWindows = skewIntervalWindows_;
    skewSampleSize = skewSampleSize_;
    skewStableK = skewStableK_;
    skewEps = skewEps_;
    skewWindowCnt = 0;

    // C/D/E 三个任务的 BloomFilter
    persistPairWinBF = BloomFilter(allTimeBFBits, bfK, 9100);
    aionPairWinBF    = BloomFilter(allTimeBFBits, bfK, 9200);
    ePairBF          = BloomFilter(allTimeBFBits, bfK, 9300);
    blacklistDFShared = BloomFilter(allTimeBFBits << 1, bfK, 9400);
    blacklistDFEx     = BloomFilter(allTimeBFBits << 1, bfK, 9500);

    exCol = std::max(1, col / TASK_NUM);

    bucket = new SharedBucket*[row];
    for (int i = 0; i < row; i++) bucket[i] = new SharedBucket[col];

    exBuckets = new ExBucket**[TASK_NUM];
    for (int t = 0; t < TASK_NUM; t++) {
        exBuckets[t] = new ExBucket*[row];
        for (int i = 0; i < row; i++) exBuckets[t][i] = new ExBucket[exCol];
    }

    fStage1Shared = new FStage1Cell*[row];
    fStage1Ex = new FStage1Cell*[row];
    for (int i = 0; i < row; ++i) {
        fStage1Shared[i] = new FStage1Cell[col];
        fStage1Ex[i] = new FStage1Cell[exCol];
    }

    // A/B/C/E 通用索引哈希
    hashFunctions = new BOBHash*[row];
    for (int i = 0; i < row; i++) hashFunctions[i] = new BOBHash(1000 + i);

    for (int t = 0; t < TASK_NUM; t++) {
        N_est[t] = 0;
        M_mass[t] = 0;
        alpha_cur[t] = ZIPF_ALPHA_INIT;
        alpha_frozen[t] = false;
        alpha_hist[t].clear();
    }
    fNEstShared = 0;
    fMMassShared = 0;
    fAlphaShared = ZIPF_ALPHA_INIT;
    fAlphaSharedFrozen = false;
    fAlphaSharedHist.clear();
    curWindowId = 0;

    for (int t = 0; t < TASK_NUM; t++) {
        samplers[t].reset((size_t)skewSampleSize, 20000ULL + (uint64_t)t * 1315423911ULL);
    }
    fSamplerShared.reset((size_t)skewSampleSize, 4000000007ULL);

    aionBH = new aion::BH_BF<uint64_t, uint64_t>();
    aionTolerance = 17;
    aionBfCounter = 1;
}

Sketch::~Sketch() {
    if (bucket) {
        for (int i = 0; i < row; i++) delete[] bucket[i];
        delete[] bucket;
    }
    if (exBuckets) {
        for (int t = 0; t < TASK_NUM; t++) {
            for (int i = 0; i < row; i++) delete[] exBuckets[t][i];
            delete[] exBuckets[t];
        }
        delete[] exBuckets;
    }
    if (hashFunctions) {
        for (int i = 0; i < row; i++) delete hashFunctions[i];
        delete[] hashFunctions;
    }
    if (fStage1Shared) {
        for (int i = 0; i < row; ++i) delete[] fStage1Shared[i];
        delete[] fStage1Shared;
    }
    if (fStage1Ex) {
        for (int i = 0; i < row; ++i) delete[] fStage1Ex[i];
        delete[] fStage1Ex;
    }
    if (aionBH) {
        delete aionBH;
        aionBH = nullptr;
    }
}

// ===================== 哈希索引 =====================
int Sketch::getBucketIndex(int row_idx, const std::string& key, bool ex) const {
    uint32_t h = hashFunctions[row_idx]->run(key.c_str(), (int)key.size());
    return ex ? (int)(h % (uint32_t)exCol) : (int)(h % (uint32_t)col);
}

// ===================== 百分位互推（Zipf） =====================
double Sketch::getPercentFast(int task, uint64_t value) const {
    if (task < 0 || task >= TASK_NUM) return -1.0;
    if (value == 0) return 0.0;

    uint64_t N = N_est[task];
    uint64_t M = M_mass[task];
    if (N == 0 || M == 0) return -1.0;

    const double alpha = getAlphaUsed(task);
    const double C = C_hat_zipf(M, N, alpha);
    if (C <= 0.0) return -1.0;

    double r_hat = std::pow(C / (double)value, 1.0 / alpha);
    r_hat = clipd(r_hat, 1.0, (double)N);
    uint64_t r = (uint64_t)std::llround(r_hat);
    r = clipu64(r, 1, N);

    double p = (double)(N - r + 1) / (double)N;
    return clip01(p);
}

double Sketch::getPercentFast128(int task, unsigned __int128 value) const {
    if (task < 0 || task >= TASK_NUM) return -1.0;
    if (value == (unsigned __int128)0) return 0.0;

    uint64_t N = N_est[task];
    uint64_t M = M_mass[task];
    if (N == 0 || M == 0) return -1.0;

    const double alpha = getAlphaUsed(task);
    const long double C = (long double)C_hat_zipf(M, N, alpha);
    if (!(C > 0.0L)) return -1.0;

    long double vd = (long double)value;
    long double ratio = C / vd;
    if (!(ratio > 0.0L)) return 0.0;

    long double r_hat_ld = std::pow((double)ratio, 1.0 / alpha);
    double r_hat = clipd((double)r_hat_ld, 1.0, (double)N);
    uint64_t r = (uint64_t)std::llround(r_hat);
    r = clipu64(r, 1, N);

    double p = (double)(N - r + 1) / (double)N;
    return clip01(p);
}

uint64_t Sketch::getValueFast(int task, double percent) const {
    if (task < 0 || task >= TASK_NUM) return 0;
    uint64_t N = N_est[task];
    uint64_t M = M_mass[task];
    if (N == 0 || M == 0) return 0;

    const double p = clip01(percent);

    const double alpha = getAlphaUsed(task);
    const double C = C_hat_zipf(M, N, alpha);
    if (C <= 0.0) return 0;

    const uint64_t k = percentile_rank_k(p, N);

    uint64_t r = N - k + 1;
    double v = C / std::pow((double)r, alpha);
    if (v < 0.0) v = 0.0;
    return (uint64_t)std::llround(v);
}

unsigned __int128 Sketch::getValueFast128(int task, double percent) const {
    if (task < 0 || task >= TASK_NUM) return (unsigned __int128)0;
    uint64_t N = N_est[task];
    uint64_t M = M_mass[task];
    if (N == 0 || M == 0) return (unsigned __int128)0;

    const double p = clip01(percent);

    const double alpha = getAlphaUsed(task);
    const long double C = (long double)C_hat_zipf(M, N, alpha);
    if (!(C > 0.0L)) return (unsigned __int128)0;

    const uint64_t k = percentile_rank_k(p, N);

    uint64_t r = N - k + 1;
    long double v = C / std::pow((long double)r, (long double)alpha);
    if (!(v > 0.0L)) return (unsigned __int128)0;
    return (unsigned __int128)(v + 0.5L);
}

// ===================== 任务重要性标量 =====================
uint64_t Sketch::bucketScalar(int task, uint64_t u1, uint64_t u2) const {
    if (task == 0 || task == 1) {
        return u2;
    }
    if (task == 2) {
        return u2;
    }
    if (task == 3) {
        return aion_scalar64(u1, u2);
    }
    if (task == 4) {
        return seg_est_card(u2);
    }
      if (task == 5) {
        return f_scalar(u1, u2);
    }
    return 0;
}

#define REMOVE_SHARED_TASK_SAMPLER(taskId, keyVal) \
    do { \
        samplers[(taskId)].remove((keyVal)); \
        N_est[(taskId)] = samplers[(taskId)].estimateDistinctN(); \
    } while (0)

#define SYNC_SHARED_TASK_SAMPLER(taskId, keyVal, scalarVal) \
    do { \
        samplers[(taskId)].remove((keyVal)); \
        if ((scalarVal) > 0ULL) samplers[(taskId)].observe((keyVal), (scalarVal)); \
        N_est[(taskId)] = samplers[(taskId)].estimateDistinctN(); \
    } while (0)

#define REMOVE_SHARED_F_SAMPLER(keyVal) \
    do { \
        fSamplerShared.remove((keyVal)); \
        fNEstShared = fSamplerShared.estimateDistinctN(); \
        N_est[5] = fNEstShared; \
    } while (0)

#define SYNC_SHARED_F_SAMPLER(keyVal, scalarVal) \
    do { \
        fSamplerShared.remove((keyVal)); \
        if ((scalarVal) > 0ULL) fSamplerShared.observe((keyVal), (scalarVal)); \
        fNEstShared = fSamplerShared.estimateDistinctN(); \
        N_est[5] = fNEstShared; \
    } while (0)
static inline uint64_t aion_delta_from_period(uint32_t period, uint32_t tolerance) {
    if (period == 0) return 0ULL;
    uint64_t d = (uint64_t)tolerance / (uint64_t)period;
    if (d == 0) d = 1ULL;
    return d;
}

// ===================== Aion 索引 =====================
static constexpr uint32_t AION_BFSEED = 1001;
static inline uint32_t aion_bucket_index(uint64_t key, int row_i, uint32_t width) {
    return (uint32_t)(aion::Hash::BOBHash64(
        (const uint8_t*)&key,
        (uint32_t)sizeof(uint64_t),
        (uint32_t)(AION_BFSEED + (uint32_t)row_i)
    ) % (uint64_t)width);
}

// ===================== shared：Stable(A/B) =====================
void Sketch::processSharedStableAB(int task, uint64_t itemKey, uint32_t pktSize, const std::string& hashKey) {
    if (task != 0 && task != 1) return;
    uint64_t delta = (task == 1) ? (uint64_t)(pktSize ? pktSize : 1u) : 1ULL;
    if(task==0)AllA++;
    else AllB++;

    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &b = bucket[i][idx];
        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.task = (uint8_t)task;
            b.u1 = 1ULL;
            b.u2 = delta;
            if(task==0) inA++;
            else inB++;
            return;
        }
        if ((int)b.task == task && b.key == itemKey) {
            b.u2 = sat_add_u64(b.u2, delta);
            b.u1 = sat_add_u64(b.u1, 1ULL);
            return;
        }
    }

    int bestRow = -1;
    int bestIdx = -1;
    double bestPct = 2.0;
    int bestVictTask = -1;
    uint64_t bestVictValue = 0;

    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &v = bucket[i][idx];
        int vt = (int)v.task;
        uint64_t value = bucketScalar(vt, v.u1, v.u2);
        double pct = (vt == 3)
            ? getPercentFast128(3, aion_get128(v.u1, v.u2))
            : getPercentFast(vt, value);
        if (pct < 0.0) continue;
        if (pct < bestPct) {
            bestPct = pct;
            bestRow = i;
            bestIdx = idx;
            bestVictTask = vt;
            bestVictValue = value;
        }
    }

    if (bestRow < 0) return;
  //if(task==0)
   // shaA++;
    //else shaB++;
    uint64_t baseI = (bestVictTask == task) ? bestVictValue : getValueFast(task, bestPct);
    if (baseI == 0) baseI = 1ULL;
    if(bestVictTask==1||bestVictTask==0) baseI=baseI*180;//*255;
    double p = (task == 1)
        ? (double)delta / (double)(baseI + delta)
        : 1.0 / (double)(baseI + 1ULL);
    if (!hitProb(p)) return;
    //  if(task==0)
    //zhaA++;
    //else zhaB++;
    SharedBucket &victim = bucket[bestRow][bestIdx];
    const uint64_t oldU1 = victim.u1;
    const uint64_t oldVictKey = victim.key;
    const int oldVictTask = (int)victim.task;
    if (oldVictTask == 5) REMOVE_SHARED_F_SAMPLER(oldVictKey);
    else if (oldVictTask >= 0 && oldVictTask < TASK_NUM) REMOVE_SHARED_TASK_SAMPLER(oldVictTask, oldVictKey);

    victim.key = itemKey;
    victim.task = (uint8_t)task;

    if (bestVictTask == task) {
        victim.u1 = (oldU1 > 0ULL) ? (oldU1 - 1ULL) : 0ULL;
        victim.u2 = (task == 0) ? 1ULL : delta;
        if(task==0)endA++;
        else endB++;
        return;
    }
    if(bestVictTask==2) {
       // std::cout<<"AB change c: "<<dol++<<std::endl;
       endC++;
        //return;
        }
     if(bestVictTask==3) {
       // std::cout<<"AB change D: "<<dod++<<std::endl;
       endD++;
         }
       if(bestVictTask==4) {
       // std::cout<<"AB change E: "<<doe++<<std::endl;
       endE++;
       }
    if (task == 0 && bestVictTask == 2) {
        victim.u1 = 1ULL;
        victim.u2 = 1ULL;
    } else if (task == 1 && bestVictTask == 2) {
        // C 被 B 替换：按用户规则，新 B 桶直接写当前到达包字节数 pktSize。
        victim.u1 = 1ULL;
        victim.u2 = delta;
    } else if (task == 0 && bestVictTask == 3) {
        victim.u1 = 1ULL;
        victim.u2 = 1ULL;
    } else if (task == 1 && bestVictTask == 3) {
        // D 被 B 替换：按用户规则，新 B 桶直接写当前到达字节增量 delta，
        // 不写成 baseB + delta。
        victim.u1 = 1ULL;
        victim.u2 = delta;
    } else if (task == 0 && bestVictTask == 4) {
        victim.u1 = 1ULL;
        victim.u2 = 1ULL;
    } else if (task == 1 && bestVictTask == 4) {
        // E 被 B 替换：按用户规则直接写入当前到达包字节数 L，而不是 baseI + L
        victim.u1 = 1ULL;
        victim.u2 = delta;
    }
     else if (task == 0 && bestVictTask == 5) {
        victim.u1 = 1ULL;
        victim.u2 = 1ULL;
    } else if (task == 1 && bestVictTask == 5) {
        victim.u1 = 1ULL;
        victim.u2 = delta;
    }else {
        victim.u1 = 1ULL;
        victim.u2 = sat_add_u64(baseI, delta);
    }
}

// ===================== shared：P-Sketch(C) =====================
void Sketch::processSharedPSketchC(int task, uint64_t itemKey, const std::string& hashKey) {

    if (task != 2) return;
    AllC++;
    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &b = bucket[i][idx];
        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.task = (uint8_t)task;
            b.u2 = 1ULL;
            b.u1 = c_pack_state(0ULL, false);
            inC++;
            return;
        }
        if ((int)b.task == task && b.key == itemKey) {
            if (!c_flag(b.u1)) return;
            uint64_t inact = c_inactivity(b.u1);
            if (inact > 0ULL) inact -= 1ULL;
            b.u2 = sat_add_u64(b.u2, 1ULL);
            b.u1 = c_pack_state(inact, false);
            return;
        }
    }

    int bestRow = -1;
    int bestIdx = -1;
    double bestPct = 2.0;
    int bestVictTask = -1;
    uint64_t bestVictValue = 0;



    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &v = bucket[i][idx];
        int vt = (int)v.task;
        uint64_t value = bucketScalar(vt, v.u1, v.u2);
        double pct = (vt == 3)
            ? getPercentFast128(3, aion_get128(v.u1, v.u2))
            : getPercentFast(vt, value);
        if (pct < 0.0) continue;
        if (pct < bestPct) {
            bestPct = pct;
            bestRow = i;
            bestIdx = idx;
            bestVictTask = vt;
            bestVictValue = value;
        }
    }
    if (bestRow < 0) return;
    //shaC++;
    SharedBucket &victim = bucket[bestRow][bestIdx];
    // 若 victim 是 C 且其 flag=False，说明该流本窗口已经到达过，放弃替换。
    if (bestVictTask == 2 && !c_flag(victim.u1)) return;

    uint64_t baseImp = (bestVictTask == 2) ? bestVictValue : getValueFast(2, bestPct);
    if (baseImp == 0) baseImp = 1ULL;

    double p = 1.0 / (double)(baseImp + 1ULL);
    if (!hitProb(p)) return;

        const uint64_t oldVictKey = victim.key;
    const int oldVictTask = (int)victim.task;
    if (oldVictTask == 5) REMOVE_SHARED_F_SAMPLER(oldVictKey);
    else if (oldVictTask >= 0 && oldVictTask < TASK_NUM) REMOVE_SHARED_TASK_SAMPLER(oldVictTask, oldVictKey);

    //zhaC++;
    //std::cout<<"victim: "<<victim.task<<" csd c:"<<del++<<std::endl;
      if(bestVictTask==1) {
        //std::cout<<"C change B: "<<dol++<<std::endl;
        endB++;
         }
        if(bestVictTask==0) {
        //std::cout<<"C change A: "<<dol++<<std::endl;
         endA++;
         }
          if(bestVictTask==2) {
       // std::cout<<"C change C: "<<dol++<<std::endl;
        //return;
        endC++;
         }
     if(bestVictTask==3) {
       // std::cout<<"C change D: "<<dod++<<std::endl;
       endD++;
         }
       if(bestVictTask==4) {
       // std::cout<<"C change E: "<<doe++<<std::endl;
       endE++;
       }
    victim.key = itemKey;
    victim.task = (uint8_t)task;
    victim.u2 = 1ULL;//baseImp +1ULL;
    victim.u1 = c_pack_state(0ULL, false);

    SYNC_SHARED_TASK_SAMPLER(task, itemKey, bucketScalar(task, victim.u1, victim.u2));
}

// ===================== shared：Aion(P) =====================
void Sketch::processSharedAionP(uint64_t itemKey, uint32_t dd) {
    const int task = 3;
    if (dd == 0) return;
    if (aionBfCounter <= (uint64_t)aionTolerance) return;
    AllD++;
    unsigned __int128 delta128 = (unsigned __int128)aion_delta_from_period(dd, aionTolerance);
    if (delta128 == (unsigned __int128)0) return;
    unsigned __int128 tcur128 = (unsigned __int128)aionBfCounter;

    for (int i = 0; i < row; i++) {
        uint32_t idx = aion_bucket_index(itemKey, i, (uint32_t)col);
        SharedBucket &b = bucket[i][(int)idx];
        if (b.key == itemKey && (int)b.task == task) {
            unsigned __int128 cnt = aion_get128(b.u1, b.u2);
            if (cnt == (unsigned __int128)0) cnt = delta128;
            uint64_t ps = (cnt == (unsigned __int128)0) ? 0ULL : (uint64_t)(tcur128 / cnt);
            if ((uint64_t)dd < ps) {
                cnt = cnt + 1;
            } else if ((uint64_t)dd > ps) {
                cnt = (tcur128 + (unsigned __int128)dd - 1) / (unsigned __int128)dd;
            } else {
                cnt = cnt + 1;
            }
            aion_set128(cnt, b.u1, b.u2);
            return;
        }
    }

    for (int i = 0; i < row; i++) {
        uint32_t idx = aion_bucket_index(itemKey, i, (uint32_t)col);
        SharedBucket &b = bucket[i][(int)idx];
        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.task = (uint8_t)task;
            aion_set128(delta128, b.u1, b.u2);
            inD++;
            return;
        }
    }

    int bestRow = -1;
    int bestIdx = -1;
    double bestPct = 2.0;
    int bestVictTask = -1;

    for (int i = 0; i < row; i++) {
        uint32_t idx = aion_bucket_index(itemKey, i, (uint32_t)col);
        SharedBucket &v = bucket[i][(int)idx];
        int vt = (int)v.task;
        double pct = -1.0;
        if (vt == 3) pct = getPercentFast128(3, aion_get128(v.u1, v.u2));
        else pct = getPercentFast(vt, bucketScalar(vt, v.u1, v.u2));
        if (pct < 0.0) continue;
        if (pct < bestPct) {
            bestPct = pct;
            bestRow = i;
            bestIdx = (int)idx;
            bestVictTask = vt;
        }
    }

    if (bestRow < 0) return;
    //shaD++;
    SharedBucket &v = bucket[bestRow][bestIdx];
    unsigned __int128 baseImp = (bestVictTask == task)
        ? aion_get128(v.u1, v.u2)
        : getValueFast128(task, bestPct);
    if (baseImp == (unsigned __int128)0) baseImp = 1;

    // 包括 E 被 D 替换在内，shared D 的替换概率统一使用 deltaD / (vD + deltaD)。
    if(bestVictTask==0||bestVictTask==1)baseImp=baseImp*180;//*255;
    long double p = (long double)delta128 / (long double)(baseImp + delta128);
    if (!hitProb((double)p)) return;
    //zhaD++;

     const uint64_t oldVictKey = v.key;
    const int oldVictTask = (int)v.task;
    if (oldVictTask == 5) REMOVE_SHARED_F_SAMPLER(oldVictKey);
    else if (oldVictTask >= 0 && oldVictTask < TASK_NUM) REMOVE_SHARED_TASK_SAMPLER(oldVictTask, oldVictKey);

 if(bestVictTask==0) {
        //std::cout<<"D change A: "<<dol++<<std::endl;
        endA++;
         }
          if(bestVictTask==1) {
        //std::cout<<"D change B: "<<dol++<<std::endl;
         endB++;
         }
          if(bestVictTask==2) {
        //std::cout<<"D change C: "<<dol++<<std::endl;
        //return;
        endC++;
         }
          if(bestVictTask==3) {
            endD++;
        //std::cout<<"D change E: "<<dol++<<std::endl;
         }
          if(bestVictTask==4) {
            endE++;
        //std::cout<<"D change E: "<<dol++<<std::endl;
         }

    v.key = itemKey;
    v.task = (uint8_t)task;
    aion_set128(baseImp + delta128, v.u1, v.u2);
    SYNC_SHARED_TASK_SAMPLER(3, itemKey, bucketScalar(3, v.u1, v.u2));
}


double Sketch::getAlphaUsedShared(int task) const {
    if (task == 5) {
        if (fAlphaSharedFrozen) return fAlphaShared;
        if (fAlphaSharedHist.empty()) return fAlphaShared;
        double s = 0.0;
        for (double a : fAlphaSharedHist) s += a;
        return s / (double)fAlphaSharedHist.size();
    }
    return getAlphaUsed(task);
}
uint64_t Sketch::getValueFastShared(int task, double percent) const {
    if (task != 5) return getValueFast(task, percent);
    uint64_t N = N_est[5];
    uint64_t M = M_mass[5];
    if (N == 0 || M == 0) return 0;

    const double p = clip01(percent);

    const double alpha = getAlphaUsedShared(task);
    const double C = C_hat_zipf(M, N, alpha);
    if (C <= 0.0) return 0;

    const uint64_t k = percentile_rank_k(p, N);

    uint64_t r = N - k + 1;
    double v = C / std::pow((double)r, alpha);
    if (v < 0.0) v = 0.0;
    return (uint64_t)std::llround(v);
}
// ===================== shared：SegSketch(E) =====================
void Sketch::processSharedSegSketchE(uint64_t hostKey, uint32_t ip_dst, const std::string& hostHashKey) {
    const int task = 4;
    AllE++;
    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hostHashKey, false);
        SharedBucket &b = bucket[i][idx];

        if (b.key == UINT64_MAX) {
            b.key = hostKey;
            b.task = (uint8_t)task;
            b.u1 = 0ULL;
            b.u2 = 0ULL;
            (void)seg_halved_update(b.u1, ip_dst);
            seg_host_update(b.u2, ip_dst, 0);
            inE++;
            return;
        }

        if ((int)b.task == task && b.key == hostKey) {
            uint32_t k = seg_halved_update(b.u1, ip_dst);
            uint32_t prefix_low = (k > 0) ? (uint32_t)((k - 1) * 8) : 0u;
            seg_host_update(b.u2, ip_dst, prefix_low);
            return;
        }
    }

    int bestRow = -1;
    int bestIdx = -1;
    double bestPct = 2.0;
    int bestVictTask = -1;
    uint64_t bestVictValue = 0;

    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hostHashKey, false);
        SharedBucket &v = bucket[i][idx];
        int vt = (int)v.task;
        uint64_t value = bucketScalar(vt, v.u1, v.u2);
        double pct = (vt == 3)
            ? getPercentFast128(3, aion_get128(v.u1, v.u2))
            : getPercentFast(vt, value);
        if (pct < 0.0) continue;
        if (pct < bestPct) {
            bestPct = pct;
            bestRow = i;
            bestIdx = idx;
            bestVictTask = vt;
            bestVictValue = value;
        }
    }

    if (bestRow < 0) return;
    //shaE++;
    // 当 victim 是 Task D 且新来的流是 E 时：先用 D 的 count 取百分位，再映射到 E 的值 v，
    // 替换成功后清空两个 bitmap，并立即对 u1/u2 重新置位。
    uint64_t v = (bestVictTask == task) ? bestVictValue : getValueFast(task, bestPct);
    if (v == 0) v = 1ULL;
        if(bestVictTask==0||bestVictTask==1)v=v*180;//*255;
    double p = 1.0 / (double)(v + 1ULL);
    if (!hitProb(p)) return;
   // zhaE++;

    SharedBucket &victim = bucket[bestRow][bestIdx];
     const uint64_t oldVictKey = victim.key;
    const int oldVictTask = (int)victim.task;
    if (oldVictTask == 5) REMOVE_SHARED_F_SAMPLER(oldVictKey);
    else if (oldVictTask >= 0 && oldVictTask < TASK_NUM) REMOVE_SHARED_TASK_SAMPLER(oldVictTask, oldVictKey);

 if(bestVictTask==0) {
        //std::cout<<"E change A: "<<dol++<<std::endl;
        endA++;
         }
          if(bestVictTask==1) {
        //std::cout<<"E change B: "<<dol++<<std::endl;
        endB++;
         }
          if(bestVictTask==2) {
        //std::cout<<"E change C: "<<dol++<<std::endl;
        endC++;
        //return;
         }
          if(bestVictTask==3) {
            endD++;
        //std::cout<<"E change D: "<<dol++<<std::endl;
         }
         if(bestVictTask==4) {
            endE++;
        //std::cout<<"E change D: "<<dol++<<std::endl;
         }

    victim.key = hostKey;
    victim.task = (uint8_t)task;
    victim.u1 = 0ULL;
    victim.u2 = 0ULL;
    (void)seg_halved_update(victim.u1, ip_dst);
    seg_host_update(victim.u2, ip_dst, 0);
}

double Sketch::getPercentFastShared(int task, uint64_t value) const {
    if (task != 5) return getPercentFast(task, value);
    if (value == 0) return 0.0;
    uint64_t N = N_est[5];
    uint64_t M = M_mass[5];
    if (N == 0 || M == 0) return -1.0;

    const double alpha = getAlphaUsedShared(task);
    const double C = C_hat_zipf(M, N, alpha);
    if (C <= 0.0) return -1.0;

    double r_hat = std::pow(C / (double)value, 1.0 / alpha);
    r_hat = clipd(r_hat, 1.0, (double)N);
    uint64_t r = (uint64_t)std::llround(r_hat);
    r = clipu64(r, 1, N);

    double p = (double)(N - r + 1) / (double)N;
    return clip01(p);
}

// ===================== shared：Burst(F) =====================
bool Sketch::processSharedBurstPromote(uint64_t itemKey, uint32_t C1, const std::string& hashKey) {
    const int task = 5;
    if (C1 == 0) return false;

    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &b = bucket[i][idx];
        if (b.key == itemKey && (int)b.task == task) {
            uint32_t cpre = f_cpre(b.u1);
            uint32_t ccur = f_ccur(b.u1);
            ccur = (uint32_t)std::min<uint64_t>((uint64_t)UINT32_MAX, (uint64_t)ccur + (uint64_t)C1);
            b.u1 = f_pack(cpre, ccur);
            SYNC_SHARED_F_SAMPLER(itemKey, f_scalar(b.u1, b.u2));
            return true;
        }
    }

    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &b = bucket[i][idx];
        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.task = (uint8_t)task;
            b.u1 = f_pack(0U, C1);
            b.u2 = f_pack_meta(0ULL, false);
            inF++;
            SYNC_SHARED_F_SAMPLER(itemKey, f_scalar(b.u1, b.u2));
            return true;
        }
    }

    int bestRow = -1, bestIdx = -1, bestVictTask = -1;
    double bestPct = 2.0;
    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &v = bucket[i][idx];
        int vt = (int)v.task;
        double pct = (vt == 3) ? getPercentFast128(3, aion_get128(v.u1, v.u2)) : getPercentFastShared(vt, bucketScalar(vt, v.u1, v.u2));
        if (pct < 0.0) continue;
        if (pct < bestPct) {
            bestPct = pct;
            bestRow = i;
            bestIdx = idx;
            bestVictTask = vt;
        }
    }
    if (bestRow < 0) return false;

    //shaF++;
    uint64_t baseF = 0ULL;
    if (bestVictTask == task) baseF = bucketScalar(task, bucket[bestRow][bestIdx].u1, bucket[bestRow][bestIdx].u2);
    else baseF = getValueFastShared(task, bestPct);
    if (baseF == 0ULL) baseF = 1ULL;

    double p = (double)C1 / (double)(baseF + (uint64_t)C1);
    if (!hitProb(p)) return false;

    //zhaF++;
    SharedBucket &victim = bucket[bestRow][bestIdx];
    const uint64_t oldVictKey = victim.key;
    const int oldVictTask = (int)victim.task;
    if (oldVictTask == 0) endA++;
    else if (oldVictTask == 1) endB++;
    else if (oldVictTask == 2) endC++;
    else if (oldVictTask == 3) endD++;
    else if (oldVictTask == 4) endE++;
    else if (oldVictTask == 5) endF++;
    if (oldVictTask == 5) REMOVE_SHARED_F_SAMPLER(oldVictKey);
    else if (oldVictTask >= 0 && oldVictTask < TASK_NUM) REMOVE_SHARED_TASK_SAMPLER(oldVictTask, oldVictKey);

    victim.key = itemKey;
    victim.task = (uint8_t)task;
    victim.u1 = f_pack(0U, C1);
    victim.u2 = f_pack_meta(0ULL, false);
    SYNC_SHARED_F_SAMPLER(itemKey, f_scalar(victim.u1, victim.u2));
    return true;
}

void Sketch::processSharedBurstF(uint64_t itemKey, const std::string& hashKey) {
    AllF++;
    fMMassShared = sat_add_u64(fMMassShared, 1ULL);
    M_mass[5] = sat_add_u64(M_mass[5], 1ULL);

    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, false);
        SharedBucket &b = bucket[i][idx];
        if (b.key == itemKey && (int)b.task == 5) {
            uint32_t cpre = f_cpre(b.u1);
            uint32_t ccur = f_ccur(b.u1);
            ccur = (uint32_t)std::min<uint64_t>((uint64_t)UINT32_MAX, (uint64_t)ccur + 1ULL);
            b.u1 = f_pack(cpre, ccur);
            SYNC_SHARED_F_SAMPLER(itemKey, f_scalar(b.u1, b.u2));
            return;
        }
    }

    uint32_t promoteC1 = 0U;
    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, false);
        FStage1Cell &c = fStage1Shared[i][idx];
        if (c.key == UINT64_MAX) {
            c.key = itemKey;
            c.cnt = 1U;
        } else if (c.key == itemKey) {
            if (c.cnt < UINT32_MAX) c.cnt += 1U;
        } else {
            if (c.cnt > 1U) c.cnt -= 1U;
            else { c.key = UINT64_MAX; c.cnt = 0U; }
        }
        if (c.key == itemKey && c.cnt >= F_STAGE1_H) {
            promoteC1 = std::max<uint32_t>(promoteC1, c.cnt);
        }
    }

    if (promoteC1 > 0U && processSharedBurstPromote(itemKey, promoteC1, hashKey)) {
        for (int i = 0; i < row; ++i) {
            int idx = getBucketIndex(i, hashKey, false);
            FStage1Cell &c = fStage1Shared[i][idx];
            if (c.key == itemKey) {
                c.key = UINT64_MAX;
                c.cnt = 0U;
            }
        }
    }
}

// ===================== exclusive：Stable(A/B) =====================
void Sketch::processExclusiveStableAB(int task, uint64_t itemKey, uint32_t pktSize, const std::string& hashKey) {
    if (task != 0 && task != 1) return;
    uint64_t delta = (task == 1) ? (uint64_t)(pktSize ? pktSize : 1u) : 1ULL;

    ExBucket **ex = exBuckets[task];

    uint64_t bestCount = UINT64_MAX;
    int bestRow = -1;
    int bestIdx = -1;
    //if(task==0)
    //norA++;
    //else norB++;
    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hashKey, true);
        ExBucket &b = ex[i][idx];

        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.u1 = 1ULL;
            b.u2 = delta;
            return;
        }
        if (b.key == itemKey) {
            b.u2 = sat_add_u64(b.u2, delta);
            b.u1 = sat_add_u64(b.u1, 1ULL);
            return;
        }
        if (b.u2 < bestCount) {
            bestCount = b.u2;
            bestRow = i;
            bestIdx = idx;
        }
    }
    if (bestRow < 0) return;
    
    //f(task==0)colA++;
    //else colB++;
    ExBucket &victim = ex[bestRow][bestIdx];
    long double strength = (long double)victim.u1 * (long double)victim.u2;
    double p = (task == 1)
        ? (double)((long double)delta / (strength + (long double)delta))
        : (double)(1.0L / (strength + 1.0L));
    if (!hitProb(p)) return;

  
    if (task == 0) {
        uint64_t oldStability = victim.u1;
        victim.key = itemKey;
        victim.u1 = (oldStability > 0ULL) ? (oldStability - 1ULL) : 0ULL;
        victim.u2 = 1ULL;
    } else {
        uint64_t oldStability = victim.u1;
        victim.key = itemKey;
        victim.u1 = (oldStability > 0ULL) ? (oldStability - 1ULL) : 0ULL;
        victim.u2 = delta;
    }
}

// ===================== exclusive：P-Sketch(C) =====================
void Sketch::processExclusivePSketchC(int task, uint64_t itemKey, const std::string& hashKey) {
    if (task != 2) return;
    ExBucket **ex = exBuckets[2];

    uint64_t minP = UINT64_MAX;
    int bestRow = -1;
    int bestIdx = -1;

        //norC++;
    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hashKey, true);
        ExBucket &b = ex[i][idx];

        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.u2 = 1ULL;
            b.u1 = c_pack_state(0ULL, false);

            return;
        }
        if (b.key == itemKey) {
            if (!c_flag(b.u1)) return;
            uint64_t inact = c_inactivity(b.u1);
            if (inact > 0ULL) inact -= 1ULL;
            b.u2 = sat_add_u64(b.u2, 1ULL);
            b.u1 = c_pack_state(inact, false);
            return;
        }

        if (b.u2 < minP) {
            minP = b.u2;
            bestRow = i;
            bestIdx = idx;
        }
    }
    if (bestRow < 0) return;

    ExBucket &victim = ex[bestRow][bestIdx];

    // 若 victim 的 flag=False，说明该流本窗口已经到达过，放弃替换。
    if (!c_flag(victim.u1)) return;
    //colC++;
    uint64_t inact = c_inactivity(victim.u1);
    uint64_t persist = victim.u2;
    uint64_t zMinus = (Sketch::C_REPLACE_Z > inact) ? (Sketch::C_REPLACE_Z - inact) : 1ULL;
    uint64_t factor = std::max<uint64_t>(1ULL, zMinus);
    long double denom = (long double)factor * (long double)persist + 1.0L;
    double p = 1.0 / (double)denom;
    if (!hitProb(p)) return;
   
    victim.key = itemKey;
    victim.u2 = 1ULL;
    victim.u1 = c_pack_state(0ULL, false);
}

// ===================== exclusive：Aion(P) =====================
void Sketch::processExclusiveAionP(uint64_t itemKey, uint32_t dd) {
    const int task = 3;
    if (dd == 0) return;
    if (aionBfCounter <= (uint64_t)aionTolerance) return;

    ExBucket **ex = exBuckets[task];
    unsigned __int128 delta128 = (unsigned __int128)aion_delta_from_period(dd, aionTolerance);
    if (delta128 == (unsigned __int128)0) return;
    unsigned __int128 tcur128 = (unsigned __int128)aionBfCounter;

    int thr = 0;
    //norD++;
    for (int i = 0; i < row; i++) {
        uint32_t idx = aion_bucket_index(itemKey, i, (uint32_t)exCol);
        ExBucket &b = ex[i][(int)idx];
        if (b.key == itemKey) {
            unsigned __int128 cnt = aion_get128(b.u1, b.u2);
            if (cnt == (unsigned __int128)0) cnt = delta128;
            uint64_t ps = (cnt == (unsigned __int128)0) ? 0ULL : (uint64_t)(tcur128 / cnt);
            if ((uint64_t)dd < ps) {
                cnt = cnt + 1;
            } else if ((uint64_t)dd > ps) {
                cnt = (tcur128 + (unsigned __int128)dd - 1) / (unsigned __int128)dd;
            } else {
                cnt = cnt + 1;
            }
            aion_set128(cnt, b.u1, b.u2);
            thr = 1;
        }
    }

    if (thr == 0) {

        for (int i = 0; i < row; i++) {
            uint32_t idx = aion_bucket_index(itemKey, i, (uint32_t)exCol);
            ExBucket &b = ex[i][(int)idx];
            if (b.key == UINT64_MAX) {
                b.key = itemKey;
                aion_set128(delta128, b.u1, b.u2);
                thr = 1;
                break;
            }
        }
    }

    if (thr == 0 && aionBfCounter > 100ULL) {
        //colD++;
        for (int i = 0; i < row && thr == 0; i++) {
            uint32_t idx = aion_bucket_index(itemKey, i, (uint32_t)exCol);
            ExBucket &b = ex[i][(int)idx];
            unsigned __int128 oldCnt = aion_get128(b.u1, b.u2);
            if (oldCnt == (unsigned __int128)0) continue;

            long double lhs = (long double)delta128 / (long double)(oldCnt + delta128);
            double rhs = (double)aion::randomGenerator() / 1000000000.0;
            //std::cout<<"rhs:"<<rhs<<" "<<lhs<<std::endl;
             
            if ((double)lhs >= rhs) {

                
                b.key = itemKey;
                aion_set128(oldCnt + delta128, b.u1, b.u2);
                thr = 1;
            }
        }
    }
}



bool Sketch::processExclusiveBurstPromote(uint64_t itemKey, uint32_t C1, const std::string& hashKey) {
    const int task = 5;
    if (C1 == 0) return false;
    ExBucket **ex = exBuckets[task];
    //norF++;
    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, true);
        ExBucket &b = ex[i][idx];
        if (b.key == itemKey) {
            uint32_t cpre = f_cpre(b.u1);
            uint32_t ccur = f_ccur(b.u1);
            ccur = (uint32_t)std::min<uint64_t>((uint64_t)UINT32_MAX, (uint64_t)ccur + (uint64_t)C1);
            b.u1 = f_pack(cpre, ccur);
            return true;
        }
    }

    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, true);
        ExBucket &b = ex[i][idx];
        if (b.key == UINT64_MAX) {
            b.key = itemKey;
            b.u1 = f_pack(0U, C1);
            b.u2 = 0ULL;
            return true;
        }
    }

    int bestRow = -1, bestIdx = -1;
    uint64_t bestVal = UINT64_MAX;
    bool haveTsZero = false;
    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, true);
        ExBucket &b = ex[i][idx];
        uint64_t score = (uint64_t)f_ccur(b.u1);
        bool tsZero = (b.u2 == 0ULL);
        if (tsZero) {
            if (!haveTsZero || score < bestVal) {
                haveTsZero = true;
                bestVal = score;
                bestRow = i;
                bestIdx = idx;
            }
        } else if (!haveTsZero && score < bestVal) {
            bestVal = score;
            bestRow = i;
            bestIdx = idx;
        }
    }
    if (bestRow < 0) return false;
   //colF++;
    ExBucket &victim = ex[bestRow][bestIdx];
    uint64_t victimScore = (uint64_t)f_ccur(victim.u1);
    if (haveTsZero) {
        if ((uint64_t)C1 <= victimScore) return false;
    } else {
        double p = (double)C1 / (double)(victimScore + (uint64_t)C1);
        if (!hitProb(p)) return false;
    }
  
    victim.key = itemKey;
    victim.u1 = f_pack(0U, C1);
    victim.u2 = f_pack_meta(0ULL, false);
    return true;
}

void Sketch::processExclusiveBurstF(uint64_t itemKey, const std::string& hashKey) {
    ExBucket **ex = exBuckets[5];

    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, true);
        ExBucket &b = ex[i][idx];
        if (b.key == itemKey) {
            uint32_t cpre = f_cpre(b.u1);
            uint32_t ccur = f_ccur(b.u1);
            ccur = (uint32_t)std::min<uint64_t>((uint64_t)UINT32_MAX, (uint64_t)ccur + 1ULL);
            b.u1 = f_pack(cpre, ccur);
            return;
        }
    }

    uint32_t promoteC1 = 0U;
    for (int i = 0; i < row; ++i) {
        int idx = getBucketIndex(i, hashKey, true);
        FStage1Cell &c = fStage1Ex[i][idx];
        if (c.key == UINT64_MAX) {
            c.key = itemKey;
            c.cnt = 1U;
        } else if (c.key == itemKey) {
            if (c.cnt < UINT32_MAX) c.cnt += 1U;
        } else {
            if (c.cnt > 1U) c.cnt -= 1U;
            else { c.key = UINT64_MAX; c.cnt = 0U; }
        }
        if (c.key == itemKey && c.cnt >= F_STAGE1_H) {
            promoteC1 = std::max<uint32_t>(promoteC1, c.cnt);
        }
    }

    if (promoteC1 > 0U && processExclusiveBurstPromote(itemKey, promoteC1, hashKey)) {
        for (int i = 0; i < row; ++i) {
            int idx = getBucketIndex(i, hashKey, true);
            FStage1Cell &c = fStage1Ex[i][idx];
            if (c.key == itemKey) {
                c.key = UINT64_MAX;
                c.cnt = 0U;
            }
        }
    }
}

// ===================== exclusive：SegSketch(E) =====================
void Sketch::processExclusiveSegSketchE(uint64_t hostKey, uint32_t ip_dst, const std::string& hostHashKey) {
    const int task = 4;
    ExBucket **ex = exBuckets[task];

    uint64_t bestCard = UINT64_MAX;
    int bestRow = -1;
    int bestIdx = -1;
    //norE++;
    for (int i = 0; i < row; i++) {
        int idx = getBucketIndex(i, hostHashKey, true);
        ExBucket &b = ex[i][idx];

        if (b.key == UINT64_MAX) {
            b.key = hostKey;
            b.u1 = 0ULL;
            b.u2 = 0ULL;
            (void)seg_halved_update(b.u1, ip_dst);
            seg_host_update(b.u2, ip_dst, 0);
            return;
        }
        if (b.key == hostKey) {
            uint32_t k = seg_halved_update(b.u1, ip_dst);
            uint32_t prefix_low = (k > 0) ? (uint32_t)((k - 1) * 8) : 0u;
            seg_host_update(b.u2, ip_dst, prefix_low);
            return;
        }

        uint64_t card = seg_est_card(b.u2);
        if (card < bestCard) {
            bestCard = card;
            bestRow = i;
            bestIdx = idx;
        }
    }
    if (bestRow < 0) return;
  //colE++;
    double p = 1.0 / (double)(bestCard + 1ULL);
    if (!hitProb(p)) return;
    
    ExBucket &victim = ex[bestRow][bestIdx];
    victim.key = hostKey;
    victim.u1 = 0ULL;
    victim.u2 = 0ULL;
    (void)seg_halved_update(victim.u1, ip_dst);
    seg_host_update(victim.u2, ip_dst, 0);
}

// ===================== 入口：BloomFilter 去重 + shared/exclusive 同时更新 =====================
void Sketch::processPacket(int task, uint64_t pairKey, uint32_t pktSize, uint32_t /*srcIp*/, const std::string& hashKey) {
    if (task < 0 || task >= TASK_NUM) return;

    uint32_t src32 = (uint32_t)(pairKey >> 32);
    uint32_t dst32 = (uint32_t)(pairKey & 0xffffffffu);

    if (task == 0) {
        M_mass[0] = sat_add_u64(M_mass[0], 1ULL);
        samplers[0].observe(pairKey, 1ULL);
        N_est[0] = samplers[0].estimateDistinctN();
    } else if (task == 1) {
        M_mass[1] = sat_add_u64(M_mass[1], (uint64_t)pktSize);
        samplers[1].observe(pairKey, (uint64_t)pktSize);
        N_est[1] = samplers[1].estimateDistinctN();
    } else if (task == 2) {
        if (persistPairWinBF.insertIfAbsent(pairKey)) {
            M_mass[2] = sat_add_u64(M_mass[2], 1ULL);
            samplers[2].observe(pairKey, 1ULL);
            N_est[2] = samplers[2].estimateDistinctN();
        }
    } else if (task == 3) {
        const bool firstInWindow = aionPairWinBF.insertIfAbsent(pairKey);
        if (firstInWindow) {
            M_mass[3] = sat_add_u64(M_mass[3], 1ULL);
            samplers[3].observe(pairKey, 1ULL);
            N_est[3] = samplers[3].estimateDistinctN();
        }

        if (!aionBH) return;

        const uint64_t ringWin = aionBfCounter % (uint64_t)aionTolerance;
        aionBH->bh->insert(pairKey, ringWin);

        uint32_t dd = 0;
        if (aionBfCounter >= (uint64_t)aionTolerance) {
            dd = (uint32_t)aionBH->bh->isperiod(pairKey, ringWin);
        }
        const bool inserted = (dd > 0);

        if (aionBfCounter > (uint64_t)aionTolerance && !inserted) {
            for (int i = 0; i < row; ++i) {
                uint32_t idx2 = aion_bucket_index(pairKey, i, (uint32_t)col);
                SharedBucket &b = bucket[i][(int)idx2];
                if (b.key == pairKey && (int)b.task == 3) {
                    b.key = UINT64_MAX;
                    b.u1 = b.u2 = 0;
                }
            }
            ExBucket **exP = exBuckets[3];
            for (int i = 0; i < row; ++i) {
                uint32_t idx2 = aion_bucket_index(pairKey, i, (uint32_t)exCol);
                ExBucket &b = exP[i][(int)idx2];
                if (b.key == pairKey) {
                    b.key = UINT64_MAX;
                    b.u1 = b.u2 = 0;
                }
            }
        }

        if (aionBfCounter > (uint64_t)aionTolerance && inserted) {
            processSharedAionP(pairKey, dd);
            processExclusiveAionP(pairKey, dd);
        }
        return;
    } else if (task == 4) {
        uint64_t hostKey = (uint64_t)src32;
        if (ePairBF.insertIfAbsent(pairKey)) {
            M_mass[4] = sat_add_u64(M_mass[4], 1ULL);
            samplers[4].observe(hostKey, 1ULL);
            N_est[4] = samplers[4].estimateDistinctN();
        }

        std::string hk = std::to_string(hostKey);
        processSharedSegSketchE(hostKey, dst32, hk);
        processExclusiveSegSketchE(hostKey, dst32, hk);
        return;
    }else if (task == 5){
        // F 的 shared / exclusive 黑名单也彻底各查各的。
        // M / 采样池 / N 只在 shared 侧维护；exclusive 侧不维护这些统计量。
        if (!blacklistDFShared.contains(pairKey)) processSharedBurstF(pairKey, hashKey);
        if (!blacklistDFEx.contains(pairKey)) processExclusiveBurstF(pairKey, hashKey);
        return;
    }

    uint64_t itemKey = pairKey;

    if (task == 0 || task == 1) {
        processSharedStableAB(task, itemKey, pktSize, hashKey);
        processExclusiveStableAB(task, itemKey, pktSize, hashKey);
        return;
    }
    if (task == 2) {
        processSharedPSketchC(task, itemKey, hashKey);
        processExclusivePSketchC(task, itemKey, hashKey);
        return;
    }
}

// ===================== 窗口结束 =====================
void Sketch::endTimeWindow() {
    persistPairWinBF.clear();
    aionPairWinBF.clear();

    // C：按用户最新定义，flag=true 表示“本窗口尚未见过该流”。
    // 因而窗口结束时：flag=true -> inactivity 加一；flag=false -> inactivity 减一（不低于 0）；
    // 随后为下一窗口统一重置为 flag=true。
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            SharedBucket &b = bucket[i][j];
            if (b.key == UINT64_MAX) continue;
            if ((int)b.task != 2) continue;
            uint64_t inact = c_inactivity(b.u1);
            bool flag = c_flag(b.u1);
            if (flag) {
                if (inact < Sketch::C_INACT_MASK) inact += 1ULL;
            } else {
                if (inact > 0ULL) inact -= 1ULL;
            }
            b.u1 = c_pack_state(inact, true);
        }
    }

    ExBucket **exC = exBuckets[2];
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < exCol; j++) {
            ExBucket &b = exC[i][j];
            if (b.key == UINT64_MAX) continue;
            uint64_t inact = c_inactivity(b.u1);
            bool flag = c_flag(b.u1);
            if (flag) {
                if (inact < Sketch::C_INACT_MASK) inact += 1ULL;
            } else {
                if (inact > 0ULL) inact -= 1ULL;
            }
            b.u1 = c_pack_state(inact, true);
        }
    }

    const uint64_t thisWin = curWindowId + 1ULL;

    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            SharedBucket &b = bucket[i][j];
            if (b.key == UINT64_MAX || (int)b.task != 5) continue;
            if (f_confirmed(b.u2)) continue;

            uint32_t cpre = f_cpre(b.u1);
            uint32_t ccur = f_ccur(b.u1);
            uint64_t ts = f_ts(b.u2);
            if (cpre < F_STAGE1_H && ccur < F_STAGE1_H) {
                REMOVE_SHARED_F_SAMPLER(b.key);
                clearSharedBucket(b);
                continue;
            }

            bool confirmedBurst = false;

            if (cpre > 0U &&
                (uint64_t)ccur * (uint64_t)F_BURST_K <= (uint64_t)cpre) {
                if (ts != 0ULL &&
                    thisWin >= ts &&
                    (thisWin - ts) <= (uint64_t)F_BURST_L) {
                    confirmedBurst = true;
                }
            }

            if (confirmedBurst) {
                blacklistDFShared.insert(b.key);
                REMOVE_SHARED_F_SAMPLER(b.key);
                b.u2 = f_pack_meta(ts, true);
                continue;
            }

            if (cpre > 0U &&
                (uint64_t)ccur >= (uint64_t)F_BURST_K * (uint64_t)cpre) {
                ts = thisWin;
            }

            if (ccur < F_BURST_T) {
                ts = 0ULL;
            }

            b.u1 = f_pack(ccur, 0U);
            b.u2 = f_pack_meta(ts, false);
            SYNC_SHARED_F_SAMPLER(b.key, f_scalar(b.u1, b.u2));
        }
    }

    ExBucket **exF = exBuckets[5];
    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < exCol; ++j) {
            ExBucket &b = exF[i][j];
            if (b.key == UINT64_MAX) continue;
            if (f_confirmed(b.u2)) continue;

            uint32_t cpre = f_cpre(b.u1);
            uint32_t ccur = f_ccur(b.u1);
            uint64_t ts = f_ts(b.u2);
            if (cpre < F_STAGE1_H && ccur < F_STAGE1_H) { clearExBucket(b); continue; }

            bool confirmedBurst = false;

            if (cpre > 0U &&
                (uint64_t)ccur * (uint64_t)F_BURST_K <= (uint64_t)cpre) {
                if (ts != 0ULL &&
                    thisWin >= ts &&
                    (thisWin - ts) <= (uint64_t)F_BURST_L) {
                    confirmedBurst = true;
                }
            }

            if (confirmedBurst) {
                blacklistDFEx.insert(b.key);
                b.u2 = f_pack_meta(ts, true);
                continue;
            }

            if (cpre > 0U &&
                (uint64_t)ccur >= (uint64_t)F_BURST_K * (uint64_t)cpre) {
                ts = thisWin;
            }

            if (ccur < F_BURST_T) {
                ts = 0ULL;
            }

            b.u1 = f_pack(ccur, 0U);
            b.u2 = f_pack_meta(ts, false);
        }
    }

    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) { fStage1Shared[i][j].key = UINT64_MAX; fStage1Shared[i][j].cnt = 0U; }
        for (int j = 0; j < exCol; ++j) { fStage1Ex[i][j].key = UINT64_MAX; fStage1Ex[i][j].cnt = 0U; }
    }

    curWindowId += 1ULL;

    aionBfCounter += 1ULL;
    if (aionBH && aionBfCounter > (uint64_t)aionTolerance) {
        aionBH->bh->find(aionBfCounter % (uint64_t)aionTolerance);
    }

    skewWindowCnt += 1ULL;
    onEndIntervalMaybeEstimate();
}

// ===================== 导出结果 =====================
std::unordered_map<uint64_t, uint64_t> Sketch::getSharedMap(int task) const {
    std::unordered_map<uint64_t, uint64_t> mp;
    if (task < 0 || task >= TASK_NUM) return mp;

    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            const SharedBucket &b = bucket[i][j];
            if (b.key == UINT64_MAX) continue;
            if ((int)b.task != task) continue;

            uint64_t v = 0;
            if (task == 0 || task == 1) v = b.u2;
            else if (task == 2) v = b.u2;
            else if (task == 3) v = aion_scalar64(b.u1, b.u2);
            else if (task == 4) {
                v = seg_est_card(b.u2);
            }
            else if (task == 5) {
                if (!f_confirmed(b.u2)) continue;
                v = 1ULL;
            }
            mp[b.key] = v;
        }
    }
    return mp;
}

std::unordered_map<uint64_t, uint64_t> Sketch::getExMap(int task) const {
    std::unordered_map<uint64_t, uint64_t> mp;
    if (task < 0 || task >= TASK_NUM) return mp;

    ExBucket **ex = exBuckets[task];
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < exCol; j++) {
            const ExBucket &b = ex[i][j];
            if (b.key == UINT64_MAX) continue;

            uint64_t v = 0;
            if (task == 0 || task == 1) v = b.u2;
            else if (task == 2) v = b.u2;
            else if (task == 3) v = aion_scalar64(b.u1, b.u2);
            else if (task == 4) {
                v = seg_est_card(b.u2);
            }else if (task == 5) {
                if (!f_confirmed(b.u2)) continue;
                v = 1ULL;
            }
            mp[b.key] = v;
        }
    }
    return mp;
}
