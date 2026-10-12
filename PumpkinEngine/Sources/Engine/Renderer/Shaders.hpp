#pragma once

#include <Renderer/Types.hpp>

namespace Pumpkin {
    std::vector<Vec4> VertexShader(const std::vector<Vec4>& vertices);
    void PixelShader(
        const FrameBuffer& fb, DepthBuffer& db,
        const std::vector<ScreenVertex>& vertices, const std::vector<Color>& colors
    );
}
