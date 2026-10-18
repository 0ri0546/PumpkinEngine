#pragma once

#include <vector>
#include <Renderer/Types.hpp>
#include <Renderer/Camera.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/Fragment.hpp>
#include <Renderer/Rasterizer.hpp>

namespace Pumpkin {
    std::vector<Vertex> VertexShader(
        const std::vector<Vertex>& vertices,
        const Camera& camera,
        float aspectRatio
    );

    template <FragmentShader_T Shader>
    void FragmentShader(
        PrimitiveBatch& batch,
        const FrameBuffer& fb,
        DepthBuffer& db,
        Shader& shader,
        const Texture& texture
    ) {
        static constexpr Rasterizer rasterizer{
            .ClearColor = 0x00000000u,
            .DepthTest = DepthTestMode::Less,
            .AlphaBlend = BlendMode::Alpha,
            .Projection = CameraType::Orthographic,
            .BilinearFiltering = true
        };
        rasterizer.Rasterize(batch, fb, db, shader, texture);
    }
}
