#pragma once

#include <Math/Vec.h>
#include <Renderer/Types.hpp>
#include <Renderer/Texture.hpp>

#include <span>

namespace Pumpkin {
    void RasterizeFrame(
        std::span<const Vertex> v,
        std::span<const uint32_t> indices,
        std::span<const Color> colors,
        const FrameBuffer& fb,
        DepthBuffer& db,
        const Texture& texture
    );
}
