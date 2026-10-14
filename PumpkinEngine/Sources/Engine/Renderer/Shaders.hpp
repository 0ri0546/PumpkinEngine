#pragma once

#include <Renderer/Types.hpp>

#include <Math/Vec.h>

namespace Pumpkin {
    std::vector<Vec4f> VertexShader(const std::vector<Vec4f>& vertices);
    void PixelShader(
        const FrameBuffer& fb, DepthBuffer& db,
        const std::vector<Vec4f>& vertices, const std::vector<Color>& colors
    );
}
