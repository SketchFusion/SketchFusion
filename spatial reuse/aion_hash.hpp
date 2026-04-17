// Minimal hash utilities for Aion (periodic flow) filter implementation.
// Interface compatible with Aion public code: Hash::BOBHash32/64 and randomGenerator().

#pragma once

#include <cstdint>
#include <random>

extern "C" {
#include "hash.h" // MurmurHash64A / MurmurHash3_x64_128
}

namespace aion {

// Thread-local RNG (public code uses a random generator during probabilistic replacement)
static thread_local std::mt19937 g_rng(std::random_device{}());

inline uint32_t randomGenerator() {
    return g_rng();
}

class Hash {
public:
    static uint32_t BOBHash32(const uint8_t* str, uint32_t len, uint32_t seed) {
        uint64_t h = MurmurHash64A(str, (int)len, (uint64_t)seed * 0x9e3779b97f4a7c15ULL);
        return (uint32_t)(h ^ (h >> 32));
    }

    static uint64_t BOBHash64(const uint8_t* str, uint32_t len, uint32_t seed) {
        uint64_t out[2] = {0, 0};
        MurmurHash3_x64_128(str, (int)len, seed, out);
        return out[0] ^ out[1];
    }
};

} // namespace aion
