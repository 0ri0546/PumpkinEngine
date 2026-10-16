#include <Renderer/Rasterizer.hpp>

#include <algorithm>
#include <limits>
#include <span>

namespace Pumpkin {
    /// @brief Clears the color and depth of one tile.
    void Rasterizer::ClearTile(const Tile& tile, float* depth, std::uint32_t* pixels, std::uint32_t width) const {
        const auto count = static_cast<std::size_t>(tile.x1 - tile.x0);
        for (int y = tile.y0; y < tile.y1; ++y) {
            const std::size_t offset = static_cast<std::size_t>(y) * width + tile.x0;
            std::ranges::fill(std::span{ depth + offset, count }, std::numeric_limits<float>::max());
            std::ranges::fill(std::span{ pixels + offset, count }, ClearColor);
        }
    }
}
