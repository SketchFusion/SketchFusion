// Bitset utilities used by Aion's BH filter (public code).

#pragma once

#include <cstdint>
#include <cstring>

namespace aion {

class BHSet {
public:
    uint64_t* bh_count;
    uint64_t* bh_sum;

    explicit BHSet(uint64_t _LENGTH) : LENGTH(_LENGTH) {
        uint64_t size = _LENGTH;
        bh_count = new uint64_t[size];
        bh_sum = new uint64_t[size];
        std::memset(bh_count, 0, size * sizeof(uint64_t));
        std::memset(bh_sum, 0, size * sizeof(uint64_t));
    }

    ~BHSet() {
        delete[] bh_count;
        delete[] bh_sum;
    }

    inline void add(uint64_t num, uint64_t sum) {
        bh_count[num]++;
        bh_sum[num] += sum;
    }

private:
    const uint64_t LENGTH;
};

class BitSet {
public:
    explicit BitSet(uint64_t _LENGTH) : LENGTH(_LENGTH) {
        uint64_t size = (_LENGTH + 7) / 8;
        bitset = new uint8_t[size];
        std::memset(bitset, 0, size);
    }
    ~BitSet() { delete[] bitset; }

    inline void Set(uint64_t index) { bitset[index >> 3] |= (1u << (index & 0x7)); }
    inline void Reset(uint64_t index) { bitset[index >> 3] &= ~(1u << (index & 0x7)); }
    inline bool Test(uint64_t index) const { return (bitset[index >> 3] >> (index & 0x7)) & 1u; }

private:
    uint8_t* bitset = nullptr;
    const uint64_t LENGTH;
};

} // namespace aion
