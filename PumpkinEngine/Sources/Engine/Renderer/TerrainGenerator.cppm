module;

#include <DLL/Export.hpp>

export module Pumpkin.Renderer.TerrainGenerator;

import Pumpkin.Renderer.Types;

import Pumpkin.Math.Vec;

import std;

export namespace Pumpkin {
    struct PUMPKIN_API TerrainConfig {
        // Grid resolution (in cells) and world-space size.
        int cellsX = 64;
        int cellsZ = 64;
        float sizeX = 20.0f;
        float sizeZ = 20.0f;

        // Height range and vertical offset of the noise.
        float minHeight = -2.0f;
        float maxHeight = 5.0f;
        float baseHeight = -1.5f;

        // Fractal noise.
        float noiseScale = 0.12f;
        int noiseOctaves = 4;
        float persistence = 0.5f;
        float lacunarity = 2.0f;
        unsigned int seed = 42;

        float waterLevel = 0.0f;
        float sandLevel = 0.15f;
        float grassLevel = 0.45f;
        float rockLevel = 0.75f;
        float snowLevel = 0.90f;

        float waterColorLevel = 0.05f;
    };

    // Axis-aligned bounding box.
    struct AABB {
        Vec3f min;
        Vec3f max;
    };

    // A patch of the terrain with its own mesh and local-space bounds.
    struct TerrainChunk {
        Mesh mesh;
        AABB bounds;
    };

    // The whole terrain as a single mesh.
    [[nodiscard]] Mesh PUMPKIN_API GenerateTerrain(const TerrainConfig& config = {});

    [[nodiscard]] Mesh PUMPKIN_API GeneratePineTrees(
        const TerrainConfig& config,
        int treeCount = 100
    );

    // The same terrain split into chunkCells x chunkCells patches
    [[nodiscard]] std::vector<TerrainChunk> PUMPKIN_API GenerateTerrainChunks(
        const TerrainConfig& config,
        int chunkCells = 16
    );

    // Flat water surface, only emitted where the terrain is below config.waterLevel.
    [[nodiscard]] Mesh PUMPKIN_API GenerateWater(const TerrainConfig& config);

    // Converts a level relative to [minHeight, maxHeight] into an absolute height.
    [[nodiscard]] float GetTerrainLevel(
        float baseHeight,
        float minHeight,
        float maxHeight,
        float relativeLevel
    );

    std::vector<Vec4f> waves;
}
