export module Pumpkin.Renderer.Rasterizer;

import Pumpkin.Renderer.Fragment;
import Pumpkin.Renderer.Types;
import Pumpkin.Renderer.PrimitiveAssembly;
import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Passes.Binning;
import Pumpkin.Renderer.RasterizerDetail;
import Pumpkin.Renderer.Texture;
import Pumpkin.Utils.ThreadPool;

import std;

export namespace Pumpkin {
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

#include "Rasterizer.inl"
