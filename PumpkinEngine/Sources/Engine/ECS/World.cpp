/// @file World.cpp
/// @brief Implementation unit of Pumpkin.ECS.World (documented in World.cppm).

module Pumpkin.ECS.World;

import std;

namespace Pumpkin::ECS {

    Entity World::CreateEntity() {
        std::uint32_t index;

        // Recycle the most recently freed slot first; its generation was already bumped on destroy.
        if (!m_freeIndices.empty()) {
            index = m_freeIndices.back();
            m_freeIndices.pop_back();
        } else {
            index = m_nextIndex++;

            if (index >= m_generations.size()) [[unlikely]] {
                const std::size_t new_capacity = std::max<std::size_t>(1024, m_generations.size() * 2);
                m_generations.resize(new_capacity, 0);
                m_alive.resize(new_capacity, 0);
            }
        }

        m_alive[index] = 1;

        return {.index = index, .generation = m_generations[index]};
    }

    void World::DestroyEntity(Entity entity) {
        if (!IsAlive(entity))
            return;

        // Strip every component, then invalidate outstanding handles by bumping the generation.
        m_registry.RemoveEntity(entity);

        m_alive[entity.index] = 0;
        ++m_generations[entity.index];
        m_freeIndices.push_back(entity.index);
    }

    bool World::IsAlive(Entity entity) const {
        return entity.index < m_generations.size() && entity.index != 0 &&
               m_generations[entity.index] == entity.generation && m_alive[entity.index];
    }

    void World::Clear() {
        // Clear all pools, then rebuild the free list (highest index first so index 1 is reused first).
        m_registry.Clear();

        // Invalidates every live handle
        m_freeIndices.clear();
        m_freeIndices.reserve(m_nextIndex - 1);
        for (std::uint32_t i = m_nextIndex - 1; i >= 1; --i) {
            if (m_alive[i]) {
                m_alive[i] = 0;
                ++m_generations[i];
            }
            m_freeIndices.push_back(i);
        }
    }
} // namespace Pumpkin::ECS
