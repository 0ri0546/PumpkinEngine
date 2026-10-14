#include "Shaders.hpp"

#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {
    // TODO(When 3D): Near clipping + projection + MVP
    std::vector<Vec4f> VertexShader(const std::vector<Vec4f>& vertices) {
        std::vector<Vec4f> clip{};
        clip.reserve(vertices.size());
        for (const auto& v : vertices) {
            clip.push_back(v);
        }
        return clip;
    }

    void PixelShader(
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<Vec4f>& vertices,
        const std::vector<Color>& colors
    ) {
        RasterizeFrame(vertices, colors, fb, db);
    }
}
