/// @file Components.cppm
/// @brief Module Pumpkin.Demo.Components: ECS component types of the demo (plain data, no behaviour).
///
/// Every type satisfies the Component concept (move-constructible, move-assignable, destructible).
/// Empty structs are used as tags.

export module Pumpkin.Demo.Components;

import Pumpkin.Demo.Config;
import Pumpkin.Renderer;
import Pumpkin.Math.Mat;
import Pumpkin.Math.Quaternion;
import Pumpkin.Math.Vec;
import std;

export namespace Pumpkin::Demo::Comp {
    // --- Transform, split so that systems writing different parts can run in parallel ---
    struct Position { Vec3f value; };
    struct Rotation { Quaternionf value; };

    /// Output of the BuildModel system; read by the renderer through DrawItem::model.
    struct ModelMatrix { Mat4f value; };

    // --- Rendering ---
    /// Non-owning: the Mesh lives in Assets (or in the TerrainBatcher).
    struct MeshRef { const Mesh* mesh = nullptr; };

    /// One Material per entity, so each entity can be animated independently.
    /// Its texture pointers are non-owning and refer to Assets.
    struct SurfaceMaterial { Material value; };

    // --- Debug / queries ---
    struct DebugName { std::string value; };
    struct TerrainTag {};

    // --- Behaviours ---
    /// Spins around world Y. `basePitch` keeps a model's fixed tilt while it spins.
    struct Spin {
        float degreesPerSecond = 0.0f;
        float yaw              = 0.0f;   // radians, accumulated by the Spin system
        float basePitch        = 0.0f;   // radians
    };

    /// Vertical sine motion around `baseY`.
    struct Bob {
        float amplitude = 0.5f;
        float speed     = 2.0f;          // radians per second
        float baseY     = 0.0f;
        float phase     = 0.0f;
    };

    /// Entity is destroyed (through a CommandBuffer) when `remaining` reaches 0.
    struct Lifetime { float remaining = 5.0f; };

    /// Added by the Lifetime system shortly before death: the entity sinks into the ground.
    struct Fading { float sinkSpeed = kSinkSpeed; };

    /// Cycles the entity's SurfaceMaterial through kShowcaseShaders.
    struct ShaderCycle {
        float       period = 4.0f;
        float       timer  = 0.0f;
        std::size_t index  = 0;
    };

    /// Singleton-style entity that periodically spawns props through CommandBuffer::Spawn.
    struct Spawner {
        const Mesh*   mesh = nullptr;
        Material      material{};
        float         interval     = 1.0f;
        float         timer        = 0.0f;
        float         halfExtent   = 10.0f;
        std::size_t   maxAlive     = kMaxSpawnedProps;
        std::uint32_t spawnedTotal = 0;
        std::mt19937  rng{ 1234 };
    };
}
