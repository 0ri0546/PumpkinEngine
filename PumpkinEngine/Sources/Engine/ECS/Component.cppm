export module Pumpkin.ECS.Component;

import std;

export namespace Pumpkin::ECS {
    template <typename T>
    concept Component = std::is_object_v<T> && std::is_move_constructible_v<T> && std::is_move_assignable_v<T> &&
                        std::is_destructible_v<T>;
} // namespace Pumpkin::ECS
