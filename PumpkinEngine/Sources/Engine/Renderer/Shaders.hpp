#pragma once

#include <Renderer/Types.hpp>

namespace Pumpkin {
    std::vector<Vec4f> VertexShader(const std::vector<Vec4f>& vertices);
    void PixelShader(
        const FrameBuffer& fb, DepthBuffer& db,
        const std::vector<Vec4f>& vertices, const std::vector<Color>& colors
    );
}
