#pragma once

#include <cstdint>
#include <utility>
#include <vector>

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

    // Output of the vertex stage
    struct Vec4 {
        float x{ 0 }, y{ 0 }, z{ 0 }, w{ 1 };
    };

    // Output of ToScreen
    struct ScreenVertex {
        float x{ 0 }, y{ 0 }, z{ 0 }, invW{ 1 };
    };

    using uint32_t2 = std::pair<uint32_t, uint32_t>;
}
