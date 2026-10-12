#include "Utils.hpp"

#include <algorithm>
#include <bit>
#include <chrono>

namespace Pumpkin {
    uint32_t ToRGBA(float r, float g, float b, float a) {
        const auto to8 = [](float v) { return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f); };
        if constexpr (std::endian::native == std::endian::little) {
            return std::byteswap(
                (to8(r) << 24) |
                (to8(g) << 16) |
                (to8(b) << 8) |
                (to8(a))
            );
        }
        else {
            return
                (to8(r) << 24) |
                (to8(g) << 16) |
                (to8(b) << 8) |
                (to8(a));
        }
    }
}
