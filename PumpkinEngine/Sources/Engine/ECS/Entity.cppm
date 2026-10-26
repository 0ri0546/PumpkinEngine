export module Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    struct Entity {
        std::uint32_t index;
        std::uint32_t generation;
        bool operator==(const Entity& b) const {
            return index == b.index && generation == b.generation;
        }
    };

    constexpr Entity NullEntity{};
} // namespace Pumpkin::ECS
