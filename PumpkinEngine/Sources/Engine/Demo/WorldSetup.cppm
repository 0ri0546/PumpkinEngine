/// @file WorldSetup.cppm
/// @brief Module Pumpkin.Demo.WorldSetup: creates the demo entities (direct World calls, main thread, outside Run()).

export module Pumpkin.Demo.WorldSetup;

import Pumpkin.Demo.Assets;
import Pumpkin.ECS;
import Pumpkin.Renderer;

export namespace Pumpkin::Demo {
    /// @brief Handles the main loop needs after the world is built.
    struct SceneHandles {
        ECS::Entity water;
        ECS::Entity terrainInside;
        ECS::Entity terrainCrossing;
        ECS::Entity trees;
        ECS::Entity cow;
        ECS::Entity vikingRoom;
        ECS::Entity spawner;
    };

    /// @brief Reserves the component pools and creates every demo entity.
    ///
    /// Components only hold non-owning pointers: @p assets, @p materials and @p terrain must outlive @p world.
    ///
    /// @param world         World to fill.
    /// @param assets        Meshes and textures referenced by the entities.
    /// @param materials     Materials copied into the entities.
    /// @param terrain       Batcher whose (stable) meshes the terrain entities point at.
    /// @param terrainConfig Used to size the spawner's area.
    /// @return Handles of the entities the main loop needs.
    [[nodiscard]] SceneHandles PopulateWorld(ECS::World& world, const Assets& assets, const Materials& materials,
                                             TerrainBatcher& terrain, const TerrainConfig& terrainConfig);
}
