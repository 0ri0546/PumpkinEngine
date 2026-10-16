#pragma once

#include <Renderer/Fragment.hpp>
#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/Types.hpp>
#include <Renderer/Texture.hpp>

#include <cstdint>

namespace Pumpkin {
    /// @brief Stage 3: tiles, depth test, blending and the calls to the fragment shader.
    ///
    /// It does not know about vertices or lighting: it gets assembled primitives
    /// and a shader that returns the color of each pixel.
    class Rasterizer {
    public:
        std::uint32_t ClearColor = 0xFF000000u;
        DepthTestMode DepthTest = DepthTestMode::Less;
        BlendMode AlphaBlend = BlendMode::Alpha;
        CameraType Projection = CameraType::Orthographic;

        /// @brief Clears the frame, then draws all the primitives, calling `shader` for each visible pixel.
        template <FragmentShader S>
        void Rasterize(const PrimitiveBatch& batch, const FrameBuffer& fb, DepthBuffer& db, const S& shader, const Texture& texture) const;

    private:
        void ClearTile(const Tile& tile, float* depth, std::uint32_t* pixels, std::uint32_t width) const;

        template <bool Transparent, FragmentShader S>
        void DrawInTile(const Primitive& t, std::uint32_t primitiveId, const Tile& tile,
            float* depth, std::uint32_t* pixels, std::uint32_t width, const S& shader, const Texture& texture) const;
    };
}

#include <Renderer/Rasterizer.inl>
