#pragma once

#include <Renderer/Types.hpp>

#include <Math/Vec.h>

namespace Pumpkin {
    std::vector<Vertex> VertexShader(const std::vector<Vertex>& vertices);
    void PixelShader(
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices,
        const std::vector<Color>& colors
    );
}
