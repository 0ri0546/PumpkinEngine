#pragma once

#include <Renderer/Types.hpp>
#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {
    std::vector<Vertex> VertexShader(const std::vector<Vertex>& vertices);
    void PixelShader(
		Rasterizer& rasterizer,
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<Vec4f>& vertices,
        const std::vector<std::uint32_t>& indices,
        const std::vector<Color>& colors
    );
}
