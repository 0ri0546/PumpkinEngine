#pragma once

#include <vector>
#include <Renderer/Types.hpp>
#include <Renderer/Camera.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/Fragment.hpp>
#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {
    struct ClipFlags { uint32_t andCode = 0x3F; uint32_t orCode = 0; };

    inline constexpr uint32_t kNearBit = 1u << 6;

    // Bitmask to tell if a point needs clipping, if none, near plane clipping is skipped
    [[nodiscard]] inline uint32_t Outcode(const Vec4f& p) {
        const float w = p[3];
        return (p[0] < -w ?  1u : 0u) | (p[0] > w ?  2u : 0u)
             | (p[1] < -w ?  4u : 0u) | (p[1] > w ?  8u : 0u)
             | (p[2] < -w ? 16u : 0u) | (p[2] > w ? 32u : 0u)
             | (p[2] < -w ? kNearBit : 0u);   // "needs near clip"
    }

    ClipFlags VertexShader(
        std::span<const Vertex> vertices,
        std::vector<Vertex>& out,
        const Mat4f& model,
        const Camera& camera,
        float aspectRatio);

    template <FragmentShader_T Shader>
    void FragmentShader(
        PrimitiveBatch& batch,
        const FrameBuffer& fb,
        DepthBuffer& db,
        Shader& shader,
        const DrawItem& item
    ) {
        static constexpr Rasterizer rasterizer{
            .Projection = CameraType::Orthographic,
            .BilinearFiltering = true
        };
        rasterizer.Rasterize(batch, fb, db, shader, item);
    }
}
