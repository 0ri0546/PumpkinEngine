#pragma once

#include <Renderer/PrimitiveAssembly.hpp>
#include <Renderer/RenderPass.hpp>

namespace Pumpkin {
    class GeometryPass final : public RenderPass {
    public:
        [[nodiscard]] std::string_view Name() const override { return "Geometry"; }
        void Execute(RenderContext& ctx) override;

    private:
        void DrawMesh(const DrawItem&, RenderContext&);

        std::vector<Vertex>   m_clip, m_clippedVertices, m_screen;
        std::vector<uint32_t> m_clippedIndices;
        std::vector<Color>    m_clippedColors;
        std::vector<Vec3f>    m_clippedNormals;
        PrimitiveBatch        m_batch;
    };
}
