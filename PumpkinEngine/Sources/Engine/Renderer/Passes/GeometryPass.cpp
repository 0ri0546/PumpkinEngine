#include "GeometryPass.hpp"

#include <Renderer/Shaders.hpp>
#include <Renderer/Texture.hpp>
#include <Renderer/Transform.hpp>
#include <Utils/RenderProfiler.hpp>
#include <Utils/ThreadPool.hpp>

#include <ranges>

namespace Pumpkin {
    void GeometryPass::Execute(RenderContext& ctx) {
        const auto dist = [&](const DrawItem& i) {
            return Distance(ctx.camera.pos, i.model->ExtractPosition());
        };
        const auto isTransparent = [](const DrawItem& i) {
            return i.material->baseColor.a < 255.0f;
        };

        std::ranges::stable_sort(ctx.scene, [&](const DrawItem& a, const DrawItem& b) {
            const bool ta = isTransparent(a), tb = isTransparent(b);
            if (ta != tb) return tb;                // opaque first
            return ta ? dist(a) > dist(b)           // transparent: far -> near
                      : dist(a) < dist(b);          // opaque: near -> far
        });

        for (auto& item : ctx.scene)
            DrawMesh(item, ctx);
    }

    // Pipeline: vertex shader -> clipping -> primitive assembly -> rasterizer -> fragment shader
    void GeometryPass::DrawMesh(const DrawItem& item, const RenderContext& ctx) {
        // 1. Vertex shader
        ClipFlags flags;
        {
            Prof::Scope scope(Prof::Stage::Vertex);
            flags = VertexShader(
                item.mesh->vertices,
                m_clip,
                *item.model,
                ctx.camera,
                static_cast<float>(ctx.fb.width) / static_cast<float>(ctx.fb.height));
        }

        // Every vertex is outside the same plane, nothing can be visible
        if (flags.andCode != 0) return;

        std::span<const Vertex>   verts   = m_clip;
        std::span<const uint32_t> indices = item.mesh->indices;
        std::span<const Color>    colors  = item.mesh->colors;
        {
            Prof::Scope scope(Prof::Stage::Clip);

            // 2. Clipping, only when something crosses the near plane
            if (flags.orCode & kNearBit) {
                if (!ClipNearPlane(m_clip, item.mesh->indices, item.mesh->colors,
                                   m_clippedVertices, m_clippedIndices, m_clippedColors))
                    return;

                verts   = m_clippedVertices;
                indices = m_clippedIndices;
                colors  = m_clippedColors;
            }
            const size_t count = verts.size();
            m_clippedNormals.resize(count);

            constexpr size_t kChunk = 16384;
            const size_t chunks = (count + kChunk - 1) / kChunk;
            const auto copyNormals = [&](const size_t chunk) {
                const size_t begin = chunk * kChunk;
                const size_t end   = std::min(count, begin + kChunk);
                for (size_t i = begin; i < end; ++i)
                    m_clippedNormals[i] = verts[i].normal;
            };
            if (chunks <= 1) copyNormals(0);
            else ParallelFor(GetPool(), chunks, copyNormals);
        }

        // 3. Screen space
        {
            Prof::Scope scope(Prof::Stage::ToScreen);
            if (!ToScreen(verts, m_screen, ctx.fb.width, ctx.fb.height))
                return;
        }

        // 4. Primitive assembly
        {
            Prof::Scope scope(Prof::Stage::Assemble);
            const AssemblyDesc desc{
                .culling = CullMode::Back,
                .width   = ctx.fb.width,
                .height  = ctx.fb.height
            };
            AssemblePrimitives(m_screen, indices, colors, desc, m_batch);
        }

        // 5. Fragment shader
        {
            Prof::Scope scope(Prof::Stage::Fragment);
            VertexColorShader vcs{};
            LambertShader lambert{};
            BlinnPhongShader blinnPhong{};
            PBRShaderMaps pbrMaps{};
            switch (item.material->shader) {
            case ShaderId::VertexColor:
                vcs.colors = colors;
                FragmentShader(m_batch, ctx.fb, ctx.db, vcs, item);
                break;
            case ShaderId::Lambert:
                lambert = LambertShader{
                    .normals = m_clippedNormals,
                    .colors = colors
                };
                FragmentShader(m_batch, ctx.fb, ctx.db, lambert, item);
                break;
            case ShaderId::BlinnPhong:
                blinnPhong = BlinnPhongShader{
                    .normals = m_clippedNormals,
                    .colors = colors
                };
                FragmentShader(m_batch, ctx.fb, ctx.db, blinnPhong, item);
                break;
            case ShaderId::PBR:
                pbrMaps = PBRShaderMaps{
                    .normals = m_clippedNormals,
                    .colors = colors,
                    .ambient = item.material->ao,
                    .metallic = item.material->metallic,
                    .roughness = item.material->roughness
                };
                FragmentShader(m_batch, ctx.fb, ctx.db, pbrMaps, item);
                break;
            default:
                break;
            }
        }
    }
}
