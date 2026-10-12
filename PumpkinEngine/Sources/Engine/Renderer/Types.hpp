#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include <Math/Vec.h>

namespace Pumpkin {
    // 8-bit color depth
    struct alignas(4) Color {
        uint8_t r{ 0 };
        uint8_t g{ 0 };
        uint8_t b{ 0 };
        uint8_t a{ 255 };
    };

    struct FrameBuffer {
        uint32_t* pixels{ nullptr };
        uint16_t width{}, height{};
    };

    struct DepthBuffer {
        std::vector<float> pixels{};
        uint16_t width{}, height{};
    };
}
