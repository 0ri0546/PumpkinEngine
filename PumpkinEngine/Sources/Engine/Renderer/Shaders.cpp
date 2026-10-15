#include "Shaders.hpp"

#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {
    // TODO(When 3D): Near clipping + projection + MVP
    std::vector<Vertex> VertexShader(const std::vector<Vertex>& vertices) {
        std::vector<Vertex> clip{};
        clip.reserve(vertices.size());
        for (const auto& v : vertices) {
            clip.push_back(v);
        }
        return clip;
    }

    void PixelShader(
		Rasterizer& rasterizer,
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices,
        const std::vector<Color>& colors
    ) {
        
        rasterizer.RasterizeFrame(vertices, indices, colors, fb, db);
    }
}
