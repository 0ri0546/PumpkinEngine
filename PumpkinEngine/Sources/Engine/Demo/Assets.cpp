module Pumpkin.Demo.Assets;

import Pumpkin.Demo.Helpers;
import Pumpkin.Renderer;
import std;

namespace Pumpkin::Demo {
    TerrainConfig MakeTerrainConfig() {
        return TerrainConfig{
            .cellsX = 64,
            .cellsZ = 64,
            .sizeX = 30.0f,
            .sizeZ = 30.0f,
            .minHeight = -2.0f,
            .maxHeight = 5.0f,
            .baseHeight = 1.5f,
            .noiseScale = 0.18f,
            .noiseOctaves = 4,
            .persistence = 0.5f,
            .lacunarity = 2.0f,
            .seed = 7,
            .waterLevel = 0.20f,
            .sandLevel = 0.35f,
            .grassLevel = 0.35f,
            .rockLevel = 0.45f,
            .snowLevel = 0.72f
        };
    }

    void LoadAssets(Assets& assets, const TerrainConfig& terrainConfig) {
        constexpr Color white{ .r = 255, .g = 255, .b = 255, .a = 255 };

        assets.cow        = LoadMesh("Assets/Models/Spot.obj", white);
        assets.vikingRoom = LoadMesh("Assets/Models/VikingRoom.obj", white);
        assets.water      = GenerateWater(terrainConfig);
        assets.trees      = GeneratePineTrees(terrainConfig, 150);

        LoadTextureOrWarn(assets.cowAlbedo,    "Assets/Textures/SpotTexture.png");
        LoadTextureOrWarn(assets.vikingAlbedo, "Assets/Textures/VikingRoom/Albedo.png");
        LoadTextureOrWarn(assets.vikingNormal, "Assets/Textures/VikingRoom/Normal.png");
        LoadTextureOrWarn(assets.vikingORM,    "Assets/Textures/VikingRoom/ORM.png");
        LoadTextureOrWarn(assets.skybox,       "Assets/Textures/Skybox.png");
    }

    Materials MakeMaterials(const Assets& assets) {
        constexpr Color white{ .r = 255, .g = 255, .b = 255, .a = 255 };

        return Materials{
            .cow = Material{
                .baseColor = white,
                .metallic = 0.7f,
                .roughness = 0.3f,
                .ao = 0.5f,
                .albedo = &assets.cowAlbedo,
                .shader = ShaderId::Lambert,
                .animatedWaterOndulation = true,
                .animatedWaterWave = true
            },
            .vikingRoom = Material{
                .baseColor = white,
                .albedo = &assets.vikingAlbedo,
                .normal = &assets.vikingNormal,
                .orm = &assets.vikingORM,
                .shader = ShaderId::PBR
            },
            .terrain = Material{
                .shader = ShaderId::Lambert,
                .animatedWaterOndulation = false
            },
            .water = Material{
                .baseColor = Color{ 35, 145, 220, 127 },
                .shader = ShaderId::Water,
                .animatedWaterOndulation = false,
                .animatedWaterWave = true
            },
            .tree = Material{
                .shader = ShaderId::BlinnPhong
            }
        };
    }
}
