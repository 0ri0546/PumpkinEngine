#include "Utils.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <random>

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

    std::vector<Color> GenerateRGBA(size_t count, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return std::vector<Color>(count, Color(r, g, b, a));
    }

    std::vector<Color> GenerateRandomRGB(size_t count) {
        static std::random_device dev{};
        static std::mt19937 rng{ dev() };
        static std::uniform_int_distribution<int> dist(0, 256);

        std::vector<Color> colors;
        colors.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            colors.emplace_back(dist(rng), dist(rng), dist(rng), 255u);
        }
        return colors;
    }

    std::vector<Color> GenerateRandomRGBA(size_t count) {
        static std::random_device dev{};
        static std::mt19937 rng{ dev() };
        static std::uniform_int_distribution<int> dist(0, 256);

        std::vector<Color> colors;
        colors.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            colors.emplace_back(dist(rng), dist(rng), dist(rng), dist(rng));
        }
        return colors;
    }
}
