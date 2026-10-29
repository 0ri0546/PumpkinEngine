/// @file Entity.cppm
/// @brief Module Pumpkin.ECS.Entity: the generational handle used to reference entities.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    /// @brief Generational handle identifying an entity inside a World.
    ///
    /// An Entity is a small value type (index + generation). The index addresses a slot in the
    /// World and in the sparse array of every ComponentPool; the generation is incremented each
    /// time a slot is recycled, so handles that outlived their entity are detected by
    /// World::IsAlive() instead of silently aliasing a newer entity.
    ///
    /// @note Index 0 is reserved: World never hands it out (see NullEntity).
    /// @thread_safety Plain value type: copying and comparing from any thread is safe.
    /// @see World::CreateEntity, World::IsAlive, NullEntity
    struct PUMPKIN_API Entity {
        /// @brief Slot number inside the World (0 is reserved for NullEntity).
        std::uint32_t index;
        /// @brief Number of times the slot was recycled; must match the World's current generation for the handle to be alive.
        std::uint32_t generation;
        /// @brief Handles are equal when both index and generation are equal.
        /// @param b Handle to compare with.
        /// @return true if @p b designates the same slot at the same generation.
        /// @note operator!= is synthesised by the compiler (C++20 rewritten candidates).
        bool operator==(const Entity& b) const {
            return index == b.index && generation == b.generation;
        }
    };

    /// @brief The "no entity" handle ({0, 0}).
    ///
    /// Index 0 is never allocated by World, so World::IsAlive(NullEntity) is always false.
    /// Use it as the default value for optional entity references.
    PUMPKIN_API constexpr Entity NullEntity{};
} // namespace Pumpkin::ECS
