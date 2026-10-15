#include "Utils.hpp"

#include <algorithm>
#include <bit>
#include <chrono>

namespace Pumpkin {
    uint32_t ToRGBA(float r, float g, float b, float a) {
        if constexpr (std::endian::native == std::endian::little) {
            return
                (static_cast<uint32_t>(a) << 24) |
                (static_cast<uint32_t>(r) << 16) |
                (static_cast<uint32_t>(g) << 8) |
                (static_cast<uint32_t>(b)
            );
        }
        else {
            return
                (static_cast<uint32_t>(r) << 24) |
                (static_cast<uint32_t>(g) << 16) |
                (static_cast<uint32_t>(b) << 8) |
                (static_cast<uint32_t>(a));
        }
    }
}
