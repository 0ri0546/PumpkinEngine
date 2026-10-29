/// @file Registry.cpp
/// @brief Implementation unit of Pumpkin.ECS.Registry (documented in Registry.cppm).

module Pumpkin.ECS.Registry;

namespace Pumpkin::ECS {
    void Registry::RemoveEntity(Entity entity) {
        // Pools are created lazily, so the table may contain empty slots.
        for (const auto& pool : m_pools)
            if (pool)
                pool->Remove(entity);
    }

    void Registry::Clear() {
        for (const auto& pool : m_pools)
            if (pool)
                pool->Clear();
    }
} // namespace Pumpkin::ECS
