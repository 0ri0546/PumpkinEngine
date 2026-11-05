module Pumpkin.Demo.WorldSetup;

import Pumpkin.Demo.Components;
import Pumpkin.Demo.Config;
import Pumpkin.Demo.Helpers;
import Pumpkin.ECS;
import Pumpkin.Renderer;
import Pumpkin.Math.Mat;
import Pumpkin.Math.Quaternion;
import Pumpkin.Math.Vec;
import std;

using namespace Pumpkin::ECS;
using namespace Pumpkin::Demo::Comp;

namespace Pumpkin::Demo {
    namespace {
        /// Entity that never moves: only a mesh, a material and a constant model matrix.
        /// (No Position/Rotation, so the BuildModel system never visits it.)
        Entity CreateStaticProp(World& world, std::string name, const Mesh* mesh, const Material& material) {
            const Entity entity = world.CreateEntity();
            world.Add<DebugName>(entity, DebugName{ std::move(name) });
            world.Add<MeshRef>(entity, MeshRef{ mesh });
            world.Add<SurfaceMaterial>(entity, SurfaceMaterial{ material });
            world.Add<ModelMatrix>(entity, ModelMatrix{ Mat4f::Identity() });
            return entity;
        }

        /// Entity with a Position/Rotation pair, so BuildModel recomputes its matrix every frame.
        Entity CreateDynamicProp(World& world, std::string name, const Mesh* mesh, const Material& material,
                                 const Vec3f& position, float pitch) {
            const Entity entity = CreateStaticProp(world, std::move(name), mesh, material);
            world.Add<Position>(entity, Position{ position });
            world.Add<Rotation>(entity, Rotation{ MakeYawPitch(0.0f, pitch) });
            return entity;
        }
    }

    SceneHandles PopulateWorld(World& world, const Assets& assets, const Materials& materials,
                               TerrainBatcher& terrain, const TerrainConfig& terrainConfig) {
        // Pre-size the pools we know will be used: no reallocation while the game runs.
        world.Reserve<Position, Rotation, ModelMatrix, MeshRef, SurfaceMaterial,
                      Spin, Bob, Lifetime, Fading>(kReservedEntities);

        SceneHandles handles{};

        // --- Static world geometry ---
        handles.water = CreateStaticProp(world, "Water", &assets.water, materials.water);

        // The terrain meshes are rebuilt every frame by TerrainBatcher::Update() but their
        // addresses are stable, so a MeshRef can point at them once and for all.
        handles.terrainInside   = CreateStaticProp(world, "Terrain (inside)",   &terrain.Inside(),   materials.terrain);
        handles.terrainCrossing = CreateStaticProp(world, "Terrain (crossing)", &terrain.Crossing(), materials.terrain);
        world.Add<TerrainTag>(handles.terrainInside, TerrainTag{});
        world.Add<TerrainTag>(handles.terrainCrossing, TerrainTag{});

        handles.trees = CreateStaticProp(world, "Pine trees", &assets.trees, materials.tree);

        // --- Animated props ---
        handles.cow = CreateDynamicProp(world, "Spot the cow", &assets.cow, materials.cow,
                                        Vec3f{ 0.0f, 7.0f, 0.0f }, 0.0f);
        world.Add<Spin>(handles.cow, Spin{ .degreesPerSecond = 40.0f, .yaw = 0.0f, .basePitch = 0.0f });
        world.Add<Bob>(handles.cow, Bob{ .amplitude = 0.5f, .speed = 2.0f, .baseY = 7.0f, .phase = 0.0f });
        world.Add<ShaderCycle>(handles.cow, ShaderCycle{ .period = 4.0f });

        constexpr float kVikingPitch = -std::numbers::pi_v<float> / 2.0f;
        handles.vikingRoom = CreateDynamicProp(world, "Viking room", &assets.vikingRoom, materials.vikingRoom,
                                               Vec3f{ -3.0f, 7.0f, -3.0f }, kVikingPitch);
        world.Add<Spin>(handles.vikingRoom,
                        Spin{ .degreesPerSecond = 15.0f, .yaw = 0.0f, .basePitch = kVikingPitch });

        // --- Spawner: creates short-lived copies of the cow through the CommandBuffer ---
        handles.spawner = world.CreateEntity();
        world.Add<DebugName>(handles.spawner, DebugName{ "Prop spawner" });
        {
            Spawner spawner{};
            spawner.mesh       = &assets.cow;
            spawner.material   = materials.cow;
            spawner.interval   = 0.8f;
            spawner.halfExtent = terrainConfig.sizeX * 0.4f;
            world.Add<Spawner>(handles.spawner, std::move(spawner));
        }

        return handles;
    }
}
