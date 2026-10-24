#pragma once

#include <Renderer/Types.hpp>

namespace Pumpkin {
    struct TerrainConfig {
        int cellsX = 64;
        int cellsZ = 64;

        float sizeX = 20.0f;
        float sizeZ = 20.0f;

        float minHeight = -2.0f;
        float maxHeight = 5.0f;

        float baseHeight = -1.5f;

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

    [[nodiscard]] Mesh GenerateTerrain(
        const TerrainConfig& config = {}
    );

    [[nodiscard]] Mesh GenerateWater(const TerrainConfig& config);
}