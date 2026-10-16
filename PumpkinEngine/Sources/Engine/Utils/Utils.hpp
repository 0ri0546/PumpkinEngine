#pragma once

#include <Renderer/Types.hpp>

#include <cstdint>
#include <vector>

namespace Pumpkin {
    uint32_t ToRGBA(float r, float g, float b, float a = 1.0f);

    // Color helpers
    std::vector<Color> GenerateRGBA(size_t count, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    std::vector<Color> GenerateRandomRGB(size_t count);
    std::vector<Color> GenerateRandomRGBA(size_t count);
}
