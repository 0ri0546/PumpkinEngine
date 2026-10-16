#pragma once

#include <vector>
#include <Renderer/Types.hpp>
#include <Renderer/Camera.hpp>
#include <Renderer/Texture.hpp>

namespace Pumpkin {
    std::vector<Vertex> VertexShader(
        const std::vector<Vertex>& vertices,
        const Camera& camera,
        float aspectRatio
    );

    void PixelShader(
        const FrameBuffer& fb,
        DepthBuffer& db,
        const std::vector<Vertex>& vertices,
        const std::vector<std::uint32_t>& indices,
        const std::vector<Color>& colors,
        const Texture& texture
    );
}