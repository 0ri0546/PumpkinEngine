export module Pumpkin.Renderer.TerrainBatcher;

import Pumpkin.Renderer.TerrainGenerator;
import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;

import std;

export namespace Pumpkin {
    // Frustum-culls terrain chunks every frame and merges the visible ones into two
    // meshes, so the pipeline only processes what is on screen:
    //   - Inside():   chunks entirely in front of the near plane (no clipping needed)
    //   - Crossing(): chunks straddling the near plane (must go through the clipper)
    // Either mesh can be empty on a given frame.
    class TerrainBatcher {
    public:
        explicit TerrainBatcher(std::vector<TerrainChunk> chunks);

        TerrainBatcher(const TerrainBatcher&) = delete;
        TerrainBatcher& operator=(const TerrainBatcher&) = delete;

        void Update(const Mat4f& mvp);

        [[nodiscard]] const Mesh& Inside() const;
        [[nodiscard]] const Mesh& Crossing() const;

    private:
        // Visible chunks of one category, merged into a single mesh.
        struct Batch {
            std::vector<const Mesh*> sources;
            std::vector<std::size_t> vtxOffset, idxOffset;
            Mesh mesh;

            void Gather();
        };

        std::vector<TerrainChunk> chunks_;
        Batch inside_, crossing_;
    };
}
