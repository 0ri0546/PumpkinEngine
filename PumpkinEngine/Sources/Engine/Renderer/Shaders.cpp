#include "Shaders.hpp"

#include <Renderer/Rasterizer.hpp>

#include <thread>

namespace Pumpkin {
    // TODO(When 3D): Near clipping + projection + MVP
    std::vector<Vec4> VertexShader(const std::vector<Vec4>& vertices) {
        std::vector<Vec4> clip{};
        clip.reserve(vertices.size());
        for (const auto& v : vertices) {
            clip.push_back(v);
        }
        return clip;
    }

    void PixelShader(
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<ScreenVertex>& vertices,
        const std::vector<Color>& colors
    ) {
        const unsigned hc = std::max(1u, std::thread::hardware_concurrency());
        const unsigned threadCount = hc > 2 ? hc - 2 : 1; // avoids underflow

        std::vector<std::pair<uint32_t2, uint32_t2>> groups{};
        groups.reserve(threadCount);

        const uint32_t rowsPerGroup = fb.height / threadCount;
        const uint32_t remainder = fb.height % threadCount;

        uint32_t y = 0;
        for (size_t i = 0; i < threadCount; ++i) {
            const uint32_t rows = rowsPerGroup + (i < remainder ? 1 : 0);
            groups.push_back({
                { 0, y },
                { fb.width, y + rows }
            });
            y += rows;
        }

        float* depth = db.pixels.data();
        uint32_t* pixels = fb.pixels;
        const uint32_t width = fb.width;

        std::vector<std::jthread> threads{};
        threads.reserve(groups.size());
        for (const auto& group : groups) {
            threads.emplace_back([group, &vertices, &colors, depth, pixels, width]() {
                Rasterize(group, vertices, colors, depth, pixels, width);
            });
        }
    }
}
