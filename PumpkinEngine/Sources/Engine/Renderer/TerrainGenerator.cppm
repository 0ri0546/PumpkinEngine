/// @file TerrainGenerator.cppm
/// @brief Module Pumpkin.Renderer.TerrainGenerator: procedural fractal-noise terrain and water meshes.

module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.TerrainGenerator;

import Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {
    /// @brief Parameters of the procedural terrain.
    ///
    /// The height field is fractal value noise remapped into `[minHeight, maxHeight]` around `baseHeight`. The biome
    /// levels (`sandLevel` ... `snowLevel`) are *relative* positions in that range, converted to heights by GetTerrainLevel().
    struct PUMPKIN_API TerrainConfig {
        /// @brief Grid resolution along X, in cells (> 0).
        int cellsX = 64;
        /// @brief Grid resolution along Z, in cells (> 0).
        int cellsZ = 64;
        /// @brief World-space extent along X (> 0).
        float sizeX = 20.0f;
        /// @brief World-space extent along Z (> 0).
        float sizeZ = 20.0f;

        /// @brief Lowest possible terrain height (must be below maxHeight).
        float minHeight = -2.0f;
        /// @brief Highest possible terrain height.
        float maxHeight = 5.0f;
        /// @brief Height around which the noise oscillates; also shifts the biome levels.
        float baseHeight = -1.5f;

        /// @brief Base noise frequency (> 0): larger values give more, smaller hills.
        float noiseScale = 0.12f;
        /// @brief Number of noise layers summed (>= 1).
        int noiseOctaves = 4;
        /// @brief Amplitude multiplier between successive octaves.
        float persistence = 0.5f;
        /// @brief Frequency multiplier between successive octaves.
        float lacunarity = 2.0f;
        /// @brief Noise seed; the same configuration and seed give the same terrain.
        unsigned int seed = 42;

        /// @brief Absolute height of the water surface used by GenerateWater().
        float waterLevel = 0.0f;
        /// @brief Relative level below which the terrain is coloured as sand.
        float sandLevel = 0.15f;
        /// @brief Relative level above which flat-enough terrain turns to rock (grass lies between sand and this level).
        float grassLevel = 0.45f;
        /// @brief Relative level above which the terrain is rock.
        float rockLevel = 0.75f;
        /// @brief Relative level above which the terrain is snow.
        float snowLevel = 0.90f;

        /// @brief Not read by the generator at the moment.
        float waterColorLevel = 0.05f;
    };

    /// @brief Axis-aligned bounding box.
    struct AABB {
        /// @brief Minimum corner.
        Vec3f min;
        /// @brief Maximum corner.
        Vec3f max;
    };

    /// @brief A patch of terrain: its mesh and the local-space bounds used for frustum culling.
    struct TerrainChunk {
        Mesh mesh;
        AABB bounds;
    };

    /// @brief Generates the whole terrain as one mesh (flat-shaded: three unique vertices per triangle, per-vertex biome colours).
    /// @param config Terrain parameters.
    /// @return The terrain mesh in model space, centred on the origin in X and Z.
    /// @throws std::invalid_argument if the configuration is invalid (non-positive cell counts, sizes or noise scale,
    ///         `maxHeight <= minHeight`, fewer than one octave).
    [[nodiscard]] Mesh PUMPKIN_API GenerateTerrain(const TerrainConfig& config = {});

    /// @brief Generates low-poly pine trees scattered over the terrain, as a single flat-shaded mesh.
    ///
    /// Builds the same height field as GenerateTerrain() (so the same @p config gives matching ground)
    /// and tries @p treeCount random positions, using a random generator seeded with `config.seed + 12345`.
    /// A candidate is rejected when:
    /// - the low-frequency forest noise there (`FractalNoise` at 0.5x `noiseScale`) is below 0.01, which keeps trees in
    ///   patches, or
    /// - the ground is at most `waterLevel + 0.15`, which keeps trees out of the water.
    ///
    /// Positions are uniform inside the terrain, at least 1 unit away from each edge. Each accepted tree is:
    /// - a 5-sided open prism for the trunk (height `0.7 * h`, radius `0.12 * w`);
    /// - three stacked 5-sided open cones for the foliage (base radius `(0.65 - 0.15 * layer) * w`,
    ///   height `0.8 * h`, layers spaced `0.45 * h` apart);
    ///
    /// with `h` random in [0.3, 0.9] and `w` random in [0.5, 0.8]. That is at most 25 triangles (75 vertices) per tree.
    /// The trunk is brown (145, 100, 65). The foliage is green (135, 220, 100), or white when
    /// `(ground - minHeight) / (maxHeight - minHeight) >= snowLevel`. Every triangle has its own vertices, a face normal
    /// and a zero texcoord, and `Mesh::colors` has one colour per vertex.
    ///
    /// @param config    Terrain parameters, the same ones passed to GenerateTerrain(). The tree placement depends on
    ///                  `seed`, the size, the height range, the noise settings, `waterLevel` and `snowLevel`.
    /// @param treeCount Number of candidate positions to try (default 100). This is an upper bound: the mesh usually
    ///                  holds fewer trees. A value <= 0 returns an empty mesh.
    /// @return Tree mesh in model space, in the same frame as the terrain (centred on the origin in X and Z), so it can
    ///         be drawn with the terrain's model matrix. Empty if no candidate was accepted.
    /// @throws std::invalid_argument if @p config is invalid (checked before @p treeCount, so an invalid config throws
    ///         even when `treeCount <= 0`).
    /// @pre `config.sizeX >= 2` and `config.sizeZ >= 2`. Below that the sampling range is inverted, which is undefined
    ///      behaviour for `std::uniform_real_distribution`.
    /// @note Deterministic: the same @p config and @p treeCount always give the same mesh.
    /// @note The snow decision uses the *normalised* height directly, whereas the terrain goes through
    ///       GetTerrainLevel(), which centres the levels on `baseHeight`. The two snow lines only coincide when
    ///       `baseHeight` is at the middle of `[minHeight, maxHeight]`. With the defaults (-1.5 in [-2, 5]) the trees
    ///       turn white at about 4.3 while the terrain turns white at about 1.3.
    /// @note Ground height is a bilinear sample of the height field, while the terrain mesh is made of triangles.
    ///       The trunk base can therefore float or sink slightly on steep cells. The trunk and foliage have no bottom caps.
    /// @complexity O(cells + treeCount): the whole height field is rebuilt on every call, so cache the result.
    /// @thread_safety Thread-safe: pure function, with a local random generator and no shared state.
    /// @see GenerateTerrain, GenerateWater, TerrainConfig
    [[nodiscard]] Mesh PUMPKIN_API GeneratePineTrees(
        const TerrainConfig& config,
        int treeCount = 100
    );

    /// @brief Generates the same terrain split into square patches, each with its own bounds (for TerrainBatcher).
    /// @param config     Terrain parameters.
    /// @param chunkCells Patch edge in cells (> 0); border patches may be smaller.
    /// @return Chunks in row-major order (Z rows, then X).
    /// @throws std::invalid_argument for an invalid configuration or `chunkCells <= 0`.
    [[nodiscard]] std::vector<TerrainChunk> PUMPKIN_API GenerateTerrainChunks(
        const TerrainConfig& config,
        int chunkCells = 16
    );

    /// @brief Generates a flat water surface at `waterLevel`, emitted only where the terrain lies below it.
    ///
    /// Cells partly below the water are clipped against the plane so the surface follows the shoreline; fully dry cells emit nothing.
    /// @param config Terrain parameters (the same used for the terrain).
    /// @return The water mesh (empty if no terrain is below the water level).
    /// @throws std::invalid_argument for an invalid configuration.
    [[nodiscard]] Mesh PUMPKIN_API GenerateWater(const TerrainConfig& config);

    /// @brief Converts a level relative to the height range into an absolute height.
    /// @param baseHeight    Height corresponding to relative level 0.5.
    /// @param minHeight     Lowest allowed height.
    /// @param maxHeight     Highest allowed height.
    /// @param relativeLevel Level in [0, 1] (0.5 = baseHeight).
    /// @return `clamp(baseHeight + (relativeLevel - 0.5) * (maxHeight - minHeight), minHeight, maxHeight)`.
    [[nodiscard]] float GetTerrainLevel(
        float baseHeight,
        float minHeight,
        float maxHeight,
        float relativeLevel
    );

    std::vector<Vec4f> waves;
}
