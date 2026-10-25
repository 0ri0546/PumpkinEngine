export module Pumpkin.ECS.Registry;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.ComponentPool;
import Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    using ComponentId = std::uint32_t;

    inline ComponentId NextComponentId() {
        static ComponentId id = 0;
        return id++;
    }

    template <Component T>
    ComponentId ComponentTypeId() {
        static const ComponentId id = NextComponentId();
        return id;
    }

    class Registry {
    public:
        Registry() = default;
        ~Registry() = default;

        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;

        Registry(Registry&&) noexcept = default;
        Registry& operator=(Registry&&) noexcept = default;

        template <Component T>
        ComponentPool<T>& Pool() {
            const auto id = ComponentTypeId<T>();

            if (id >= m_pools.size())
                m_pools.resize(id + 1);

            if (!m_pools[id])
                m_pools[id] = std::make_unique<ComponentPool<T>>();

            return static_cast<ComponentPool<T>&>(*m_pools[id]);
        }

        template <Component T>
        const ComponentPool<T>& Pool() const {
            return static_cast<const ComponentPool<T>&>(*m_pools.at(ComponentTypeId<T>()));
        }

        template <Component T>
        ComponentPool<T>* TryPool() {
            const auto id = ComponentTypeId<T>();

            if (id >= m_pools.size() || !m_pools[id])
                return nullptr;

            return static_cast<ComponentPool<T>*>(m_pools[id].get());
        }

        template <Component T>
        const ComponentPool<T>* TryPool() const {
            const auto id = ComponentTypeId<T>();

            if (id >= m_pools.size() || !m_pools[id])
                return nullptr;

            return static_cast<const ComponentPool<T>*>(m_pools[id].get());
        }

        template <Component T>
        bool HasPool() const {
            const auto id = ComponentTypeId<T>();

            return id < m_pools.size() && m_pools[id] != nullptr;
        }

        template <Component... Components>
        std::vector<Entity> Query() {
            if constexpr (sizeof...(Components) == 0)
                return {};

            if (!(HasPool<Components>() && ...))
                return {};

            const std::array<IComponentPool*, sizeof...(Components)> pools{&Pool<Components>()...};

            auto* smallest = pools.front();

            for (auto* pool : pools) {
                if (pool->Entities().size() < smallest->Entities().size())
                    smallest = pool;
            }

            std::vector<Entity> result;
            result.reserve(smallest->Entities().size());

            for (const auto entity : smallest->Entities()) {
                if ((Pool<Components>().Contains(entity) && ...))
                    result.push_back(entity);
            }

            return result;
        }

        template <Component... Components>
        std::vector<Entity> QueryAny() {
            if constexpr (sizeof...(Components) == 0)
                return {};

            std::size_t max_id = 0;
            auto track_max = [&]<typename T>() {
                if (HasPool<T>()) {
                    const auto& entities = Pool<T>().Entities();
                    if (!entities.empty()) {
                        max_id = std::max(max_id, static_cast<std::size_t>(entities.back().index));
                    }
                }
            };
            (track_max.template operator()<Components>(), ...);

            if (max_id == 0)
                return {};

            std::vector<bool> visited(max_id + 1, false);
            std::vector<Entity> result;
            result.reserve(max_id + 1);

            auto collect = [&]<typename T>() {
                if (!HasPool<T>())
                    return;
                for (const auto entity : Pool<T>().Entities()) {
                    const std::size_t idx = entity.index;
                    if (!visited[idx]) {
                        visited[idx] = true;
                        result.push_back(entity);
                    }
                }
            };

            (collect.template operator()<Components>(), ...);

            return result;
        }

        template <Component... Components>
        class QueryView {
        public:
            struct Iterator {
                using value_type = std::tuple<Entity, Components&...>;

                std::span<const Entity>::iterator it;
                std::span<const Entity>::iterator end;
                std::tuple<ComponentPool<Components>*...> pools;

                void SkipToValid() {
                    while (it != end && !(std::get<ComponentPool<Components>*>(pools)->Contains(*it) && ...))
                        ++it;
                }

                Iterator& operator++() {
                    ++it;
                    SkipToValid();
                    return *this;
                }

                bool operator!=(const Iterator& other) const {
                    return it != other.it;
                }
                bool operator==(const Iterator& other) const {
                    return it == other.it;
                }

                value_type operator*() const {
                    const Entity entity = *it;
                    return value_type(entity, std::get<ComponentPool<Components>*>(pools)->Get(entity)...);
                }
            };

            QueryView(std::span<const Entity> driver, ComponentPool<Components>*... pools)
                : m_driver(driver),
                  m_pools(pools...) {}

            Iterator begin() const {
                Iterator iter{m_driver.begin(), m_driver.end(), m_pools};
                iter.SkipToValid();
                return iter;
            }

            Iterator end() const {
                return {m_driver.end(), m_driver.end(), m_pools};
            }

        private:
            std::span<const Entity> m_driver;
            std::tuple<ComponentPool<Components>*...> m_pools;
        };

        template <Component... Components>
        QueryView<Components...> View() {
            static_assert(sizeof...(Components) > 0, "View requires at least one component");

            if (!(HasPool<Components>() && ...))
                return QueryView<Components...>({}, static_cast<ComponentPool<Components>*>(nullptr)...);

            const std::array<IComponentPool*, sizeof...(Components)> pools{&Pool<Components>()...};

            auto* smallest = pools.front();
            for (auto* pool : pools) {
                if (pool->Entities().size() < smallest->Entities().size())
                    smallest = pool;
            }

            return QueryView<Components...>(smallest->Entities(), &Pool<Components>()...);
        }

        void RemoveEntity(Entity entity);

        void Clear();

    private:
        std::vector<std::unique_ptr<IComponentPool>> m_pools;
    };
} // namespace Pumpkin::ECS
