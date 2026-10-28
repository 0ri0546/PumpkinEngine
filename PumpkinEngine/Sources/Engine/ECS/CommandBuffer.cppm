module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.CommandBuffer;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.World;

import std;

export namespace Pumpkin::ECS {
    // Create/destroy entity & add/remove component cannot happen while
    // systems run in parallel
    // The scheduler applies the commands single-threaded after the frame
    class PUMPKIN_API CommandBuffer {
        using Command = std::function<void(World&)>;

    public:
        void Destroy(Entity entity) {
            Push([entity](World& world) { world.DestroyEntity(entity); });
        }

        template <Component T, typename... Args>
        void Add(Entity entity, Args&&... args) {
            Push([entity, values = std::make_tuple(std::forward<Args>(args)...)](World& world) mutable {
                if (!world.IsAlive(entity))
                    return;
                std::apply([&](auto&&... a) { world.Add<T>(entity, std::move(a)...); }, std::move(values));
            });
        }

        template <Component T>
        void Remove(Entity entity) {
            Push([entity](World& world) {
                if (world.IsAlive(entity))
                    world.Remove<T>(entity);
            });
        }

        // The entity does not exist yet when recording, init receives it when the buffer is applied
        void Spawn(std::function<void(World&, Entity)> init) {
            Push([init = std::move(init)](World& world) mutable { init(world, world.CreateEntity()); });
        }

        void Apply(World& world) {
            std::vector<Command> commands;
            {
                std::scoped_lock lock(m_mutex);
                commands.swap(m_commands);
            }
            for (auto& command : commands)
                command(world);
        }

    private:
        // Locked because a system may record from several ParallelForEach chunks at once.
        // (Order between chunks is then unspecified.)
        void Push(Command command) {
            std::scoped_lock lock(m_mutex);
            m_commands.push_back(std::move(command));
        }

        std::mutex m_mutex;
        std::vector<Command> m_commands;
    };
} // namespace Pumpkin::ECS
