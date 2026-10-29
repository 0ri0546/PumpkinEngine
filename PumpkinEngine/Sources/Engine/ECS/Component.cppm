/// @file Component.cppm
/// @brief Module Pumpkin.ECS.Component: the concept constraining ECS component types.

export module Pumpkin.ECS.Component;

import std;

export namespace Pumpkin::ECS {
    /// @brief Requirements a type must satisfy to be stored as an ECS component.
    ///
    /// A component must be an object type that is move-constructible, move-assignable and
    /// destructible. Copyability and default-construction are NOT required.
    /// Move-assignment is needed because SparseSet::Remove() fills the hole with the last
    /// element (swap-and-pop).
    ///
    /// @tparam T Candidate component type.
    template <typename T>
    concept Component = std::is_object_v<T> && std::is_move_constructible_v<T> && std::is_move_assignable_v<T> &&
                        std::is_destructible_v<T>;
} // namespace Pumpkin::ECS
