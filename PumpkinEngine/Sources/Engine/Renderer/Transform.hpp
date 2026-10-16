#pragma once

#include <Renderer/Types.hpp>

#include <span>
#include <vector>

namespace Pumpkin {
    /// @brief Perspective divide + viewport for every position, in parallel.
    /// @return false if a vertex is behind the camera (w <= 0).
    [[nodiscard]] bool ToScreen(std::span<const Vertex> clip, std::vector<Vertex>& out, int width, int height);
}
