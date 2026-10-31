/// @file Rasterizer.cppm
/// @brief Module Pumpkin.Renderer.Rasterizer: tiled, multithreaded, SIMD fragment stage (template definitions in Rasterizer.inl).

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
    ///
    /// Work is split in `kTileSize` x `kTileSize` pixel tiles; each tile is owned by a single worker
    /// thread, so no two threads ever write the same pixel and no atomics are needed on the buffers.
    ///
    /// @thread_safety Not reentrant: the tile bins live in shared static storage. Run one Rasterize() at a time.
    class Rasterizer {
    public:
        /// @brief Projection the rasterizer assumes.
        ///
        /// With Perspective, each pixel span is clipped against the camera's near/far planes using the depth plane;
        /// with Orthographic no such clipping is done.
        CameraType    Projection        = CameraType::Orthographic;
        /// @brief Samples albedo / normal / ORM maps with bilinear filtering (wrap addressing) instead of nearest.
        bool          BilinearFiltering = true;

        /// @brief Draws a batch of assembled primitives, calling `shader` for each pixel that passes the depth test.
        ///
        /// Steps: bin the primitives into tiles (BinPrimitives), then in parallel (one tile at a time per
        /// worker) sort the tile's opaque primitives front-to-back and its transparent ones back-to-front, and
        /// rasterize them: 8 pixels per SIMD step for coverage, depth test and attribute interpolation, with a
        /// scalar call of @p shader per covered pixel.
        /// - Opaque primitives write colour and depth.
        /// - Transparent primitives are blended with the material's BlendMode and never write depth.
        /// - Every written pixel also updates `ctx.toonMask` (1 if the material uses ShaderId::Toon, otherwise 0).
        ///
        /// @tparam S Fragment shader type (see FragmentShader_T).
        /// @param batch  Primitives produced by AssemblePrimitives().
        /// @param ctx    Frame data; colour, depth and toon mask are written.
        /// @param shader Shader invoked per pixel (read-only: its members act as uniforms).
        /// @param item   Draw item providing the Material (depth test, blend mode, texture maps); `item.material` must be non-null.
        /// @pre `ctx.fb.pixels` is valid, `ctx.db.pixels` and `ctx.toonMask` hold at least `width * height` entries.
        /// @note Does not clear anything: run ClearPass first. Returns immediately for an empty frame buffer.
        /// @thread_safety Blocks until all tiles are done; uses the engine thread pool. Not reentrant (see class note).
        template <FragmentShader_T S>
        void Rasterize(const PrimitiveBatch& batch, RenderContext& ctx, const S& shader, const DrawItem& item) const;

    private:
        /// @brief Rasterizes one primitive inside one tile.
        /// @tparam Transparent Selects the blending path (true) or the opaque depth/colour write path (false).
        /// @tparam S           Fragment shader type.
        /// @param t           Assembled primitive.
        /// @param primitiveId Id forwarded to the shader through FragmentInput::primitiveId.
        /// @param tile        Pixel rectangle the call is restricted to.
        /// @param ctx         Frame data.
        /// @param shader      Fragment shader.
        /// @param item        Draw item (material).
        template <bool Transparent, FragmentShader_T S>
        void DrawInTile(const Primitive& t, std::uint32_t primitiveId, const Tile& tile,
            RenderContext& ctx, const S& shader, const DrawItem& item) const;
    };
}

#include "Rasterizer.inl"
