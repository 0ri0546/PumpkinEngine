#pragma once

#include <Renderer/Fragment.hpp>
#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/Types.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/RenderPass.hpp>

#include <cstdint>

namespace Pumpkin {
    /// @brief Rasterizer does: depth test, blending and the calls to the fragment shader.
    class Rasterizer {
    public:
        DepthTestMode DepthTest         = DepthTestMode::Less;
        BlendMode     AlphaBlend        = BlendMode::Alpha;
        CameraType    Projection        = CameraType::Orthographic;
        bool          BilinearFiltering = true;

        /// @brief Clears the frame, then draws all the primitives, calling `shader` for each visible pixel.
        template <FragmentShader_T S>
        void Rasterize(const PrimitiveBatch& batch, const FrameBuffer& fb, DepthBuffer& db, const S& shader, const DrawItem& item) const;

    private:
        template <bool Transparent, FragmentShader_T S>
        void DrawInTile(const Primitive& t, std::uint32_t primitiveId, const Tile& tile,
            float* depth, std::uint32_t* pixels, std::uint32_t width, const S& shader, const DrawItem& item) const;
    };
}

#include <Renderer/Rasterizer.inl>
