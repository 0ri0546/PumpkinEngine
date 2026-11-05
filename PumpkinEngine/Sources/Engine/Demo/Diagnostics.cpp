module Pumpkin.Demo.Diagnostics;

import Pumpkin.Demo.Components;
import Pumpkin.Demo.Helpers;
import Pumpkin.ECS;
import Pumpkin.Renderer;
import std;

using namespace Pumpkin::ECS;
using namespace Pumpkin::Demo::Comp;

namespace Pumpkin::Demo {
    /// Prints a snapshot of the world using the query API.
    void ReportWorld(World& world, const SceneHandles& handles) {
        const auto renderables = world.Query<MeshRef, ModelMatrix, SurfaceMaterial>();   // all components
        const auto animated    = world.QueryAny<Spin, Bob>();                            // any component
        const auto terrainPart = world.Query<TerrainTag>();
        const auto fading      = world.Query<Fading>();

        // Registry-level access: TryPool never creates a pool.
        const auto* lifetimes = world.GetRegistry().TryPool<Lifetime>();
        const std::size_t spawnedAlive = lifetimes ? lifetimes->Values().size() : 0;

        std::println("[ECS] renderables={} animated={} terrain-chunks={} props-alive={} fading={}",
                     renderables.size(), animated.size(), terrainPart.size(), spawnedAlive, fading.size());

        // Checked lookups by handle.
        if (world.IsAlive(handles.cow)) {
            const auto* name    = world.TryGet<DebugName>(handles.cow);
            const auto* surface = world.TryGet<SurfaceMaterial>(handles.cow);
            if (name && surface) {
                std::println("[ECS] {} -> shader {} | bobbing: {}",
                             name->value, ShaderName(surface->value.shader), world.Has<Bob>(handles.cow));
            }
        }
    }

    /// Direct World edits are fine on the main thread *between* frames (the scheduler is idle).
    /// Inside a system the same change would go through ctx.Commands().
    void ToggleCowBob(World& world, Entity cow) {
        if (!world.IsAlive(cow)) {
            return;
        }

        if (world.Has<Bob>(cow)) {
            world.Remove<Bob>(cow);
            return;
        }

        const Position& position = world.Get<Position>(cow);   // throws if missing: the cow always has one
        world.Add<Bob>(cow, Bob{ .amplitude = 0.5f, .speed = 2.0f, .baseY = position.value[1], .phase = 0.0f });
    }

    std::string CowShaderName(World& world, Entity cow) {
        const auto* surface = world.TryGet<SurfaceMaterial>(cow);
        return surface ? ShaderName(surface->value.shader) : "-";
    }
}
