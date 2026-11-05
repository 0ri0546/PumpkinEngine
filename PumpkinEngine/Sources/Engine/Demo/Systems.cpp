module Pumpkin.Demo.Systems;

import Pumpkin.Demo.Components;
import Pumpkin.Demo.Config;
import Pumpkin.Demo.Helpers;
import Pumpkin.ECS;
import Pumpkin.Renderer;
import Pumpkin.Math.Mat;
import Pumpkin.Math.Quaternion;
import Pumpkin.Math.Vec;
import std;

using namespace Pumpkin;
using namespace Pumpkin::ECS;
using namespace Pumpkin::Demo;
using namespace Pumpkin::Demo::Comp;

namespace {
    /// Spins entities: rewrites Rotation from the accumulated yaw. Parallel per entity.
    void SpinSystem(SystemContext& ctx) {
        const float dt = ctx.DeltaTime();
        ctx.ParallelForEach<Rotation, Spin>([dt](Entity, Rotation& rotation, Spin& spin) {
            spin.yaw += ToRadians(spin.degreesPerSecond) * dt;
            rotation.value = MakeYawPitch(spin.yaw, spin.basePitch);
        }, 64);
    }

    /// Vertical sine motion. Parallel per entity.
    void BobSystem(SystemContext& ctx) {
        const float dt = ctx.DeltaTime();
        ctx.ParallelForEach<Position, Bob>([dt](Entity, Position& position, Bob& bob) {
            bob.phase += bob.speed * dt;
            position.value[1] = bob.baseY + bob.amplitude * std::sin(bob.phase);
        }, 64);
    }

    /// Entities in their last seconds sink into the ground. Bob/Spin were removed from them
    /// when fading started, so this system is the only one still moving them.
    void FadeSystem(SystemContext& ctx) {
        const float dt = ctx.DeltaTime();
        ctx.ParallelForEach<Position, Fading>([dt](Entity, Position& position, Fading& fading) {
            position.value[1] -= fading.sinkSpeed * dt;
        }, 64);
    }

    /// Counts lifetimes down, and records structural changes into the command buffer:
    ///  - Add<Fading> + Remove<Spin> + Remove<Bob> when the end is near,
    ///  - Destroy when it is over.
    /// Recording is thread-safe, so it can happen from ParallelForEach chunks.
    void LifetimeSystem(SystemContext& ctx) {
        const float dt = ctx.DeltaTime();
        const auto& fading = ctx.Read<Fading>();   // read-only pool access, checked against Access
        CommandBuffer& commands = ctx.Commands();

        ctx.ParallelForEach<Lifetime>([&](Entity entity, Lifetime& lifetime) {
            lifetime.remaining -= dt;

            if (lifetime.remaining <= 0.0f) {
                commands.Destroy(entity);
            } else if (lifetime.remaining <= kFadeSeconds && !fading.Contains(entity)) {
                commands.Add<Fading>(entity, Fading{});
                commands.Remove<Spin>(entity);
                commands.Remove<Bob>(entity);
            }
        }, 64);
    }

    /// Periodically spawns a prop. The entity does not exist yet while recording: the lambda
    /// given to CommandBuffer::Spawn runs when the buffer is applied (single-threaded) and
    /// receives the World and the freshly created Entity.
    void SpawnerSystem(SystemContext& ctx) {
        const std::size_t alive = ctx.Read<Lifetime>().Values().size();   // pool span, no query needed

        for ([[maybe_unused]] auto [entity, spawner] : ctx.View<Spawner>()) {
            spawner.timer += ctx.DeltaTime();
            if (spawner.timer < spawner.interval) {
                continue;
            }
            spawner.timer -= spawner.interval;

            if (alive >= spawner.maxAlive || spawner.mesh == nullptr) {
                continue;
            }

            // Randomness is drawn here, in the (single-threaded) system, then captured by value.
            std::uniform_real_distribution<float> extent(-spawner.halfExtent, spawner.halfExtent);
            std::uniform_real_distribution<float> height(6.0f, 9.0f);
            std::uniform_real_distribution<float> spinSpeed(-90.0f, 90.0f);
            std::uniform_real_distribution<float> bobSpeed(1.0f, 3.0f);
            std::uniform_real_distribution<float> bobAmplitude(0.2f, 0.6f);
            std::uniform_real_distribution<float> lifetime(5.0f, 9.0f);

            const Vec3f position{ extent(spawner.rng), height(spawner.rng), extent(spawner.rng) };
            const Spin  spin{ .degreesPerSecond = spinSpeed(spawner.rng) };
            const Bob   bob{ .amplitude = bobAmplitude(spawner.rng), .speed = bobSpeed(spawner.rng),
                             .baseY = position[1], .phase = 0.0f };
            const Lifetime life{ .remaining = lifetime(spawner.rng) };

            // Every prop gets its *own* material copy, with a different shader.
            Material material = spawner.material;
            material.shader = kShowcaseShaders[spawner.spawnedTotal % kShowcaseShaders.size()];

            const Mesh* mesh = spawner.mesh;
            const std::string name = std::format("Prop #{}", spawner.spawnedTotal);
            ++spawner.spawnedTotal;

            ctx.Commands().Spawn([=](World& world, Entity created) {
                world.Add<DebugName>(created, DebugName{ name });
                world.Add<MeshRef>(created, MeshRef{ mesh });
                world.Add<SurfaceMaterial>(created, SurfaceMaterial{ material });
                world.Add<ModelMatrix>(created, ModelMatrix{ Mat4f::Identity() });
                world.Add<Position>(created, Position{ position });
                world.Add<Rotation>(created, Rotation{ MakeYawPitch(0.0f, 0.0f) });
                world.Add<Spin>(created, spin);
                world.Add<Bob>(created, bob);
                world.Add<Lifetime>(created, life);
            });
        }
    }

    /// Switches the shader of every entity that has a ShaderCycle, using the pool API directly
    /// (Entities() / Fetch() / TryGet()) instead of a view.
    void ShaderCycleSystem(SystemContext& ctx) {
        const float dt = ctx.DeltaTime();
        auto& cycles   = ctx.Write<ShaderCycle>();
        auto& surfaces = ctx.Write<SurfaceMaterial>();

        for (const Entity entity : cycles.Entities()) {
            SurfaceMaterial* surface = surfaces.TryGet(entity);
            if (surface == nullptr) {
                continue;
            }

            ShaderCycle& cycle = cycles.Fetch(entity);   // unchecked: the entity comes from this pool
            cycle.timer += dt;
            if (cycle.timer >= cycle.period) {
                cycle.timer -= cycle.period;
                cycle.index = (cycle.index + 1) % kShowcaseShaders.size();
                surface->value.shader = kShowcaseShaders[cycle.index];
            }
        }
    }

    /// Composes Position + Rotation into the matrix the renderer reads. Runs after every
    /// system that writes Position or Rotation (the scheduler derives that from Access).
    void BuildModelSystem(SystemContext& ctx) {
        ctx.ParallelForEach<Position, Rotation, ModelMatrix>(
            [](Entity, const Position& position, const Rotation& rotation, ModelMatrix& model) {
                model.value = Mat4f::Translate(position.value);
                model.value.SetRotation(rotation.value);
            }, 64);
    }
}

namespace Pumpkin::Demo {
    /// Registration order is the *semantic* order between systems that conflict.
    void RegisterSystems(Scheduler& scheduler) {
        scheduler.AddSystem("Spawner",
            Access{}.Write<Spawner>().Read<Lifetime>(), SpawnerSystem);

        scheduler.AddSystem("Lifetime",
            Access{}.Write<Lifetime>().Read<Fading>(), LifetimeSystem);

        scheduler.AddSystem("Spin",
            Access{}.Write<Rotation, Spin>(), SpinSystem);

        scheduler.AddSystem("Bob",
            Access{}.Write<Position, Bob>(), BobSystem);

        scheduler.AddSystem("Fade",
            Access{}.Write<Position>().Read<Fading>(), FadeSystem);

        scheduler.AddSystem("ShaderCycle",
            Access{}.Write<ShaderCycle, SurfaceMaterial>(), ShaderCycleSystem);

        scheduler.AddSystem("BuildModel",
            Access{}.Read<Position, Rotation>().Write<ModelMatrix>(), BuildModelSystem);
    }
}
