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
        CameraType    Projection        = CameraType::Orthographic;
        bool          BilinearFiltering = true;

        /// @brief Clears the frame, then draws all the primitives, calling `shader` for each visible pixel.
        template <FragmentShader_T S>
        void Rasterize(const PrimitiveBatch& batch, RenderContext& ctx, const S& shader, const DrawItem& item) const;

    private:
        template <bool Transparent, FragmentShader_T S>
        void DrawInTile(const Primitive& t, std::uint32_t primitiveId, const Tile& tile,
            RenderContext& ctx, const S& shader, const DrawItem& item) const;
    };
}

#include <Renderer/Rasterizer.inl>
