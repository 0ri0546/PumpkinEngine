module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    struct PUMPKIN_API Entity {
        std::uint32_t index;
        std::uint32_t generation;
        bool operator==(const Entity& b) const {
            return index == b.index && generation == b.generation;
        }
    };

    PUMPKIN_API constexpr Entity NullEntity{};
} // namespace Pumpkin::ECS
