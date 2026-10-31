/// @file GeometryPass.cppm
/// @brief Module Pumpkin.Renderer.Passes.Geometry: the 3D pipeline (vertex shader, clipping, assembly, rasterization).

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.Passes.Geometry;

import Pumpkin.Renderer.PrimitiveAssembly;
import Pumpkin.Renderer.RenderPass;
import Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {
    /// @brief Draws every DrawItem of the scene through the full software pipeline.
    ///
    /// Per item: vertex shader -> trivial reject (all vertices outside one plane) -> near-plane clipping if needed ->
    /// screen-space conversion -> primitive assembly (back-face culling) -> rasterization with the fragment shader selected by
    /// `Material::shader`. Intermediate buffers are members, reused from frame to frame.
    ///
    /// @note Shader light parameters are not set per frame: the built-in shaders keep their default light direction/colour,
    ///       interpreted in view space (the vertex stage outputs view-space normals). Only normals, colours and, for PBR,
    ///       `ambient` (= Material::ao), `metallic` and `roughness` come from the item.
    /// @thread_safety Not reentrant (member buffers and static shader instances); parallel internally.
    class PUMPKIN_API GeometryPass final : public RenderPass {
    public:
        /// @brief Profiler name. @return "Geometry".
        [[nodiscard]] std::string_view Name() const override { return "Geometry"; }
        /// @brief Sorts the scene then draws it.
        ///
        /// Order (stable sort, in place on `ctx.scene`): opaque items first, nearest to farthest from the camera, then transparent
        /// items (`Material::baseColor.a < 255`) farthest to nearest, by distance to the model matrix translation.
        ///
        /// @param ctx Frame data; `ctx.scene` is reordered.
        /// @pre Every DrawItem has non-null mesh, model and material.
        void Execute(RenderContext& ctx) override;

    private:
        /// @brief Runs the whole pipeline for one item.
        /// @param item Object to draw.
        /// @param ctx  Frame data.
        void DrawMesh(const DrawItem&, RenderContext&);

        std::vector<Vertex>   m_clip, m_clippedVertices, m_screen;
        std::vector<std::uint32_t> m_clippedIndices;
        std::vector<Color>    m_clippedColors;
        std::vector<Vec3f>    m_clippedNormals;
        PrimitiveBatch        m_batch;
    };
}
