/// @file TerrainBatcher.cppm
/// @brief Module Pumpkin.Renderer.TerrainBatcher: per-frame frustum culling and merging of terrain chunks.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.TerrainBatcher;

import Pumpkin.Renderer.TerrainGenerator;
import Pumpkin.Renderer.Types;

import Pumpkin.Math.Mat;

import std;

export namespace Pumpkin {
    /// @brief Frustum-culls terrain chunks every frame and merges the visible ones into two meshes.
    ///
    /// - Inside(): chunks entirely in front of the near plane (no clipping needed).
    /// - Crossing(): chunks straddling the near plane (must go through the clipper).
    ///
    /// Either mesh can be empty on a given frame, and chunks completely outside the frustum are dropped.
    ///
    /// @thread_safety Not thread-safe; Update() merges meshes in parallel on the engine thread pool.
    class PUMPKIN_API TerrainBatcher {
    public:
        /// @brief Takes ownership of the chunks to manage.
        /// @param chunks Terrain patches (see GenerateTerrainChunks()).
        explicit TerrainBatcher(std::vector<TerrainChunk> chunks);

        /// @brief Non-copyable (the merged meshes point into the owned chunks).
        TerrainBatcher(const TerrainBatcher&) = delete;
        TerrainBatcher& operator=(const TerrainBatcher&) = delete;

        /// @brief Classifies every chunk's bounding box and rebuilds the two merged meshes.
        /// @param mvp Model-view-projection matrix of the terrain for this frame (same one the vertex shader will use).
        /// @post Inside() and Crossing() reflect the chunks visible with @p mvp.
        /// @warning References previously returned by Inside() / Crossing() are overwritten.
        void Update(const Mat4f& mvp);

        /// @brief Merged mesh of the chunks fully in front of the near plane. @return Mesh valid until the next Update().
        [[nodiscard]] const Mesh& Inside() const;
        /// @brief Merged mesh of the chunks crossing the near plane. @return Mesh valid until the next Update().
        [[nodiscard]] const Mesh& Crossing() const;

    private:
        /// @brief Visible chunks of one category, merged into a single mesh.
        struct Batch {
            std::vector<const Mesh*> sources;
            std::vector<std::size_t> vtxOffset, idxOffset;
            Mesh mesh;

            /// @brief Concatenates `sources` into `mesh`, offsetting indices; copies chunks in parallel.
            void Gather();
        };

        std::vector<TerrainChunk> chunks_;
        Batch inside_, crossing_;
    };
}
