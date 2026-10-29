/// @file CommandBuffer.cppm
/// @brief Module Pumpkin.ECS.CommandBuffer: deferred structural changes recorded by systems.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.CommandBuffer;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.World;

import std;

export namespace Pumpkin::ECS {
    /// @brief Records structural World changes so they can be applied later, single-threaded.
    ///
    /// Creating/destroying entities and adding/removing components cannot happen while systems
    /// run in parallel (it would reallocate pools under the readers' feet). Systems therefore
    /// record those operations here and the Scheduler calls Apply() once all systems of the
    /// frame have finished. Each scheduled system owns its own buffer, so commands are applied
    /// in system registration order.
    ///
    /// @note Arguments given to Add() are decay-copied/moved into the buffer at record time.
    /// @thread_safety Recording methods (Destroy, Add, Remove, Spawn) are thread-safe (internal
    /// mutex), e.g. from several ParallelForEach chunks; the relative order between chunks is
    /// then unspecified. Apply() must not run concurrently with anything touching the World.
    class PUMPKIN_API CommandBuffer {
        using Command = std::function<void(World&)>;

    public:
        /// @brief Records the destruction of an entity.
        /// @param entity Entity to destroy. If it is already dead when the buffer is applied, the command is a no-op.
        /// @thread_safety Thread-safe.
        /// @see World::DestroyEntity
        void Destroy(Entity entity) {
            Push([entity](World& world) { world.DestroyEntity(entity); });
        }

        /// @brief Records the addition of a component to an entity.
        ///
        /// @tparam T    Component type to add.
        /// @tparam Args Constructor argument types (stored by value).
        /// @param entity Target entity.
        /// @param args   Arguments forwarded to T's constructor when the command is applied.
        /// @note Skipped silently if @p entity is dead at apply time. If the entity already has a
        ///       T, the existing component is kept untouched (see World::Add / SparseSet::Emplace).
        /// @thread_safety Thread-safe.
        template <Component T, typename... Args>
        void Add(Entity entity, Args&&... args) {
            Push([entity, values = std::make_tuple(std::forward<Args>(args)...)](World& world) mutable {
                if (!world.IsAlive(entity))
                    return;
                std::apply([&](auto&&... a) { world.Add<T>(entity, std::move(a)...); }, std::move(values));
            });
        }

        /// @brief Records the removal of a component from an entity.
        /// @tparam T Component type to remove.
        /// @param entity Target entity. Skipped silently if it is dead at apply time; no-op if it has no T.
        /// @thread_safety Thread-safe.
        template <Component T>
        void Remove(Entity entity) {
            Push([entity](World& world) {
                if (world.IsAlive(entity))
                    world.Remove<T>(entity);
            });
        }

        /// @brief Records the creation of a new entity.
        ///
        /// The entity does not exist while recording; it is created when the buffer is applied and
        /// @p init is then called with the World and the fresh handle so it can attach components.
        ///
        /// @param init Callback invoked at apply time as `init(world, newEntity)`.
        /// @thread_safety Thread-safe.
        /// @see World::CreateEntity
        void Spawn(std::function<void(World&, Entity)> init) {
            Push([init = std::move(init)](World& world) mutable { init(world, world.CreateEntity()); });
        }

        /// @brief Executes every recorded command on @p world, in recording order, then empties the buffer.
        ///
        /// @param world World the commands are applied to.
        /// @pre No system is running (call it single-threaded, after the frame's systems finished).
        /// @warning If a command throws, the exception propagates and the remaining commands of
        ///          that batch are discarded.
        /// @thread_safety The pending list is swapped out under the mutex, so commands recorded
        ///                during Apply() are kept for the next call. Execution itself is not
        ///                synchronised with other World users.
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
        /// @brief Appends a command under the lock (shared by all recording methods).
        void Push(Command command) {
            std::scoped_lock lock(m_mutex);
            m_commands.push_back(std::move(command));
        }

        std::mutex m_mutex;
        std::vector<Command> m_commands;
    };
} // namespace Pumpkin::ECS
