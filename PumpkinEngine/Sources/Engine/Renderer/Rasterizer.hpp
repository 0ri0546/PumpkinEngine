#pragma once

#include "Types.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace Pumpkin {
    void Rasterize(
        const std::pair<uint32_t2, uint32_t2>& group,
        const std::vector<ScreenVertex>& v,
        const std::vector<Color>& colors,
        float* depth,
        uint32_t* pixels,
        uint32_t width
    );
}
