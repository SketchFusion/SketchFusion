// Aion public code uses an Abstract base class that provides a hash() wrapper.

#pragma once

#include <cstdint>
#include "aion_hash.hpp"

namespace aion {

template<typename DATA_TYPE, typename COUNT_TYPE>
class Abstract {
public:
    Abstract() = default;
    virtual ~Abstract() = default;

    inline uint32_t hash(DATA_TYPE data, uint32_t seed = 0) {
        return Hash::BOBHash32((uint8_t*)&data, (uint32_t)sizeof(DATA_TYPE), seed);
    }
};

} // namespace aion
