/// @file Assets.cppm
/// @brief Module Pumpkin.Demo.Assets: meshes, textures and materials owned by the demo.

export module Pumpkin.Demo.Assets;

import Pumpkin.Renderer;
import std;

export namespace Pumpkin::Demo {
    /// Everything components point to. Must outlive the World and must not be
    /// moved or copied once entities reference it.
    struct Assets {
        Mesh    cow;
        Mesh    vikingRoom;
        Mesh    water;
        Mesh    trees;

        Texture cowAlbedo;
        Texture vikingAlbedo;
        Texture vikingNormal;
        Texture vikingORM;
        Texture skybox;
    };

    struct Materials {
        Material cow;
        Material vikingRoom;
        Material terrain;
        Material water;
        Material tree;
    };

    /// @brief Terrain parameters used by the demo (also used to size the spawn area).
    [[nodiscard]] TerrainConfig MakeTerrainConfig();

    /// @brief Loads every mesh and texture, and generates the water and tree meshes.
    /// @param assets        Receives the data; its addresses must stay stable once entities reference it.
    /// @param terrainConfig Terrain the water and trees are generated for.
    /// @throws std::runtime_error if a model file cannot be read (from LoadMesh).
    void LoadAssets(Assets& assets, const TerrainConfig& terrainConfig);

    /// @brief Builds the materials, pointing at the textures of @p assets.
    [[nodiscard]] Materials MakeMaterials(const Assets& assets);
}
