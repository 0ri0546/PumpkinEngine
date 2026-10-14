#pragma once

#include <Math/Vec.h>
#include <Renderer/Types.hpp>

#include <span>

namespace Pumpkin {
    void RasterizeFrame(
        std::span<const Vec4f> vertices,
        std::span<const Color> colors,
        const FrameBuffer& fb,
        DepthBuffer& db
    );
}
