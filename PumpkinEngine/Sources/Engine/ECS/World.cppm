export module Pumpkin.ECS.World;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.Registry;

import std;

export namespace Pumpkin::ECS {
    class World {
    public:
        Entity CreateEntity();
        void DestroyEntity(Entity entity);

        bool IsAlive(Entity entity) const;

        void Clear();

        template <typename... Components>
        void Reserve(std::size_t capacity) {
            (m_registry.Pool<Components>().Reserve(capacity), ...);
        }

        template <Component T, typename... Args>
        T& Add(Entity entity, Args&&... args) {
            return m_registry.Pool<T>().Emplace(entity, std::forward<Args>(args)...);
        }

        template <Component T>
        void Remove(Entity entity) {
            m_registry.Pool<T>().Remove(entity);
        }

        template <Component T>
        bool Has(Entity entity) const {
            const auto* pool = m_registry.TryPool<T>();
            return pool && pool->Contains(entity);
        }

        template <Component T>
        T& Get(Entity entity) {
            return m_registry.Pool<T>().Get(entity);
        }

        template <Component T>
        const T& Get(Entity entity) const {
            return m_registry.Pool<T>().Get(entity);
        }

        template <Component T>
        T* TryGet(Entity entity) {
            auto* pool = m_registry.TryPool<T>();
            return pool ? pool->TryGet(entity) : nullptr;
        }

        template <Component T>
        const T* TryGet(Entity entity) const {
            const auto* pool = m_registry.TryPool<T>();
            return pool ? pool->TryGet(entity) : nullptr;
        }

        template <Component... Components>
        std::vector<Entity> Query() {
            return m_registry.Query<Components...>();
        }

        template <Component... Components>
        std::vector<Entity> QueryAny() {
            return m_registry.QueryAny<Components...>();
        }

        template <Component... Components>
        Registry::QueryView<Components...> View() {
            return m_registry.View<Components...>();
        }

    private:
        std::uint32_t m_nextIndex = 1;
        std::vector<std::uint32_t> m_freeIndices;
        std::vector<std::uint32_t> m_generations;
        std::vector<std::uint8_t> m_alive;
        Registry m_registry;
    };
} // namespace Pumpkin::ECS
