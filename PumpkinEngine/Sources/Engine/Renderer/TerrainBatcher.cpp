module Pumpkin.Renderer.TerrainBatcher;

import Pumpkin.Renderer.TerrainGenerator;
import Pumpkin.Renderer.Shaders;
import Pumpkin.Utils.ThreadPool;

import Pumpkin.Math.Vec;

import std;

namespace Pumpkin {
    namespace {
        enum class BoxVisibility { Outside, Inside, CrossesNear };

        // Tests the 8 corners of a box against the frustum in clip space.
        BoxVisibility ClassifyBox(const AABB& box, const Mat4f& mvp) {
            std::uint32_t andCode = 0x3F;
            bool behindNear = false;

            for (int i = 0; i < 8; ++i) {
                const Vec4f p = mvp * Vec4f{ (i & 1) ? box.max[0] : box.min[0],
                                             (i & 2) ? box.max[1] : box.min[1],
                                             (i & 4) ? box.max[2] : box.min[2], 1.0f };
                andCode &= Outcode(p);
                behindNear |= (p[2] + p[3] < 0.0f);
            }

            if (andCode != 0) return BoxVisibility::Outside; // all corners outside one plane
            return behindNear ? BoxVisibility::CrossesNear : BoxVisibility::Inside;
        }
    }

    TerrainBatcher::TerrainBatcher(std::vector<TerrainChunk> chunks)
        : chunks_(std::move(chunks)) {}

    void TerrainBatcher::Update(const Mat4f& mvp) {
        inside_.sources.clear();
        crossing_.sources.clear();

        for (const TerrainChunk& chunk : chunks_) {
            switch (ClassifyBox(chunk.bounds, mvp)) {
                case BoxVisibility::Outside:     break;
                case BoxVisibility::Inside:      inside_.sources.push_back(&chunk.mesh); break;
                case BoxVisibility::CrossesNear: crossing_.sources.push_back(&chunk.mesh); break;
            }
        }

        inside_.Gather();
        crossing_.Gather();
    }

    const Mesh& TerrainBatcher::Inside() const { return inside_.mesh; }
    const Mesh& TerrainBatcher::Crossing() const { return crossing_.mesh; }

    void TerrainBatcher::Batch::Gather() {
        const std::size_t count = sources.size();
        vtxOffset.resize(count);
        idxOffset.resize(count);

        // Prefix sums: where each chunk writes in the merged mesh.
        std::size_t vertexTotal = 0, indexTotal = 0;
        for (std::size_t k = 0; k < count; ++k) {
            vtxOffset[k] = vertexTotal;
            idxOffset[k] = indexTotal;
            vertexTotal += sources[k]->vertices.size();
            indexTotal += sources[k]->indices.size();
        }

        mesh.vertices.resize(vertexTotal);
        mesh.colors.resize(vertexTotal);
        mesh.indices.resize(indexTotal);
        if (count == 0) return;

        ParallelFor(GetPool(), count, [&](std::size_t k) {
            const Mesh& src = *sources[k];
            std::ranges::copy(src.vertices, mesh.vertices.begin() + vtxOffset[k]);
            std::ranges::copy(src.colors,   mesh.colors.begin()   + vtxOffset[k]);

            // Chunk indices are local to the chunk, shift them to the merged vertex array.
            const auto base = static_cast<std::uint32_t>(vtxOffset[k]);
            std::uint32_t* dst = mesh.indices.data() + idxOffset[k];
            for (std::size_t j = 0; j < src.indices.size(); ++j)
                dst[j] = src.indices[j] + base;
        });
    }
}
