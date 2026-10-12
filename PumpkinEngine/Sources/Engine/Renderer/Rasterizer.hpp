#pragma once

#include "Types.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace Pumpkin {
    void Rasterize(
        const std::pair<Vec2u, Vec2u>& group,
        const std::vector<Vec4f>& v,
        const std::vector<Color>& colors,
        float* depth,
        uint32_t* pixels,
        uint32_t width
    );
}
