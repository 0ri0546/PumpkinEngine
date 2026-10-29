/// @file Registry.cppm
/// @brief Module Pumpkin.ECS.Registry: component type ids, per-type pools and multi-component queries.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.Registry;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.ComponentPool;
import Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    /// @brief Dense runtime identifier of a component type (index into the Registry's pool table).
    using ComponentId = std::uint32_t;

    /// @brief Hands out the next unused ComponentId.
    /// @return A process-wide unique, monotonically increasing id.
    /// @thread_safety Thread-safe (atomic counter).
    /// @note Ids reflect first-use order and may differ between runs: never persist them.
    inline ComponentId PUMPKIN_API NextComponentId() {
        static std::atomic<ComponentId> id = 0;
        return id.fetch_add(1, std::memory_order_relaxed);
    }

    /// @brief Returns the ComponentId of type @p T, assigning one on first call.
    /// @tparam T Component type.
    /// @return Stable id for @p T for the lifetime of the process.
    /// @thread_safety Thread-safe (function-local static initialisation).
    /// @warning The id lives in a function-local static: every binary using the same Registry
    ///          must resolve to the same instantiation (watch DLL boundaries), or ids diverge.
    template <Component T>
    ComponentId PUMPKIN_API ComponentTypeId() {
        static const ComponentId id = NextComponentId();
        return id;
    }

    /// @brief Owns one ComponentPool per component type and implements multi-component queries.
    ///
    /// Pools are created lazily and stored in a table indexed by ComponentId. Queries iterate
    /// the smallest involved pool and probe the others.
    ///
    /// @thread_safety Pool creation (Pool<T>() on a missing pool) resizes the table and is NOT
    ///                thread-safe; the Scheduler pre-creates all declared pools single-threaded
    ///                (Access::EnsurePools). Afterwards, per-entity access from different
    ///                threads is safe as long as no structural change happens.
    class PUMPKIN_API Registry {
    public:
        /// @brief Creates an empty registry (no pools).
        Registry() = default;
        ~Registry() = default;

        /// @brief Non-copyable (owns pools uniquely).
        Registry(const Registry&) = delete;
        Registry& operator=(const Registry&) = delete;

        /// @brief Movable.
        Registry(Registry&&) noexcept = default;
        Registry& operator=(Registry&&) noexcept = default;

        /// @brief Returns the pool of @p T, creating it if needed.
        /// @tparam T Component type.
        /// @return Mutable reference to the pool.
        /// @warning May grow the pool table: not safe to call concurrently with other registry access.
        template <Component T>
        ComponentPool<T>& Pool() {
            const auto id = ComponentTypeId<T>();

            if (id >= m_pools.size())
                m_pools.resize(id + 1);

            if (!m_pools[id])
                m_pools[id] = std::make_unique<ComponentPool<T>>();

            return static_cast<ComponentPool<T>&>(*m_pools[id]);
        }

        /// @brief Returns the existing pool of @p T (read-only).
        /// @tparam T Component type.
        /// @return Const reference to the pool.
        /// @throws std::out_of_range if T's id is beyond the pool table.
        /// @warning If the table covers T's id but the pool was never created, this dereferences an
        ///          empty pointer (undefined behaviour). Check HasPool() or use TryPool() first.
        template <Component T>
        const ComponentPool<T>& Pool() const {
            return static_cast<const ComponentPool<T>&>(*m_pools.at(ComponentTypeId<T>()));
        }

        /// @brief Returns the pool of @p T without creating it.
        /// @tparam T Component type.
        /// @return Pointer to the pool, or nullptr if it does not exist yet.
        template <Component T>
        ComponentPool<T>* TryPool() {
            const auto id = ComponentTypeId<T>();

            if (id >= m_pools.size() || !m_pools[id])
                return nullptr;

            return static_cast<ComponentPool<T>*>(m_pools[id].get());
        }

        /// @brief Read-only variant of TryPool().
        /// @tparam T Component type.
        /// @return Const pointer to the pool, or nullptr if it does not exist yet.
        template <Component T>
        const ComponentPool<T>* TryPool() const {
            const auto id = ComponentTypeId<T>();

            if (id >= m_pools.size() || !m_pools[id])
                return nullptr;

            return static_cast<const ComponentPool<T>*>(m_pools[id].get());
        }

        /// @brief Tests whether the pool of @p T exists.
        /// @tparam T Component type.
        /// @return true if a pool was created for @p T.
        template <Component T>
        bool HasPool() const {
            const auto id = ComponentTypeId<T>();

            return id < m_pools.size() && m_pools[id] != nullptr;
        }

        /// @brief Collects the entities that have all the listed components.
        /// @tparam Components Required component types (distinct).
        /// @return Snapshot of matching entities; empty if the pack is empty or any pool is missing.
        /// @complexity O(n_min * k) with n_min the size of the smallest pool.
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

        /// @brief Collects the entities that have at least one of the listed components.
        /// @tparam Components Component types.
        /// @return Snapshot without duplicates (deduplicated by Entity::index), in pool then dense order.
        template <Component... Components>
        std::vector<Entity> QueryAny() {
            if constexpr (sizeof...(Components) == 0)
                return {};

            std::vector<bool> visited; // indexed by Entity::index, grown on demand
            std::vector<Entity> result;

            auto collect = [&]<typename T>() {
                if (!HasPool<T>())
                    return;
                for (const auto entity : Pool<T>().Entities()) {
                    const std::size_t idx = entity.index;
                    if (idx >= visited.size())
                        visited.resize(std::max(idx + 1, visited.size() * 2), false);
                    if (!visited[idx]) {
                        visited[idx] = true;
                        result.push_back(entity);
                    }
                }
            };

            (collect.template operator()<Components>(), ...);

            return result;
        }

        /// @brief Non-owning, forward-iterable view over entities having all of @p Components.
        ///
        /// Iteration is driven by the smallest pool (the "driver"); entities lacking one of the other
        /// components are skipped. Dereferencing yields `std::tuple<Entity, Components&...>`, so it
        /// works with structured bindings: `for (auto [e, pos, vel] : view)`.
        ///
        /// @tparam Components Component types of the view (distinct: they are looked up by type).
        /// @warning Invalidated by any structural change to the involved pools. Changing the
        ///          structure while iterating is undefined behaviour.
        /// @thread_safety Distinct Slice()s may be iterated on different threads; each entity then
        ///                belongs to exactly one slice (this is how ParallelForEach works).
        template <Component... Components>
        class QueryView {
        public:
            /// @brief Input-style iterator over the driver pool that skips entities missing any component.
            struct Iterator {
                using value_type = std::tuple<Entity, Components&...>;

                std::span<const Entity>::iterator it;
                std::span<const Entity>::iterator end;
                std::tuple<ComponentPool<Components>*...> pools;
                const IComponentPool* driverPool;

                /// @brief Advances to the first entity (from the current one) present in every pool, or to the end.
                void SkipToValid() {
                    const auto has = [this](const IComponentPool* pool) {
                        return pool == driverPool || pool->Contains(*it);
                    };
                    while (it != end && !(has(std::get<ComponentPool<Components>*>(pools)) && ...))
                        ++it;
                }

                /// @brief Moves to the next matching entity. @return *this.
                Iterator& operator++() {
                    ++it;
                    SkipToValid();
                    return *this;
                }

                /// @brief Inequality on the driver position. @param other Iterator to compare. @return true if the positions differ.
                bool operator!=(const Iterator& other) const {
                    return it != other.it;
                }
                /// @brief Equality on the driver position. @param other Iterator to compare. @return true if both point to the same driver position.
                bool operator==(const Iterator& other) const {
                    return it == other.it;
                }

                /// @brief Dereferences to the entity and references to its components.
                /// @return Tuple `(Entity, Components&...)`.
                /// @pre The iterator is not at end() and was positioned by SkipToValid() (always true for iterators from begin()/++).
                value_type operator*() const {
                    const Entity entity = *it;
                    // Fetch is safe, membership was validated by SkipToValid
                    return value_type(entity, std::get<ComponentPool<Components>*>(pools)->Fetch(entity)...);
                }
            };

            /// @brief Builds a view from an explicit driver.
            /// @param driverPool Pool providing the iteration order (skipped during membership checks); nullptr for an empty view.
            /// @param driver     Entities to iterate (normally driverPool->Entities(), or a sub-range of it).
            /// @param pools      Pools of every component in the view.
            QueryView(const IComponentPool* driverPool, std::span<const Entity> driver, ComponentPool<Components>*... pools)
                : m_driverPool(driverPool),
                  m_driver(driver),
                  m_pools(pools...) {}

            /// @brief Iterator to the first matching entity (== end() if none). @return Begin iterator.
            Iterator begin() const {
                Iterator iter{m_driver.begin(), m_driver.end(), m_pools, m_driverPool};
                iter.SkipToValid();
                return iter;
            }

            /// @brief Past-the-end iterator. @return End iterator.
            Iterator end() const {
                return {m_driver.end(), m_driver.end(), m_pools, m_driverPool};
            }

            /// @brief Size of the driver range.
            /// @return Number of entities in the driver pool: an *upper bound* on the number of matches, not the exact count.
            std::size_t Size() const { return m_driver.size(); }

            /// @brief Sub-view over a contiguous part of the driver range.
            /// @param first Offset in the driver range.
            /// @param count Number of driver entries to cover.
            /// @pre `first + count <= Size()` (std::span::subspan precondition).
            /// @return View over `[first, first + count)` of the driver; filtering still applies.
            QueryView Slice(std::size_t first, std::size_t count) const {
                return QueryView(m_driverPool, m_driver.subspan(first, count),
                                 std::get<ComponentPool<Components>*>(m_pools)...);
            }

        private:
            const IComponentPool* m_driverPool;
            std::span<const Entity> m_driver;
            std::tuple<ComponentPool<Components>*...> m_pools;
        };

        /// @brief Builds a QueryView over entities having all listed components.
        /// @tparam Components Required component types (distinct, at least one: enforced by static_assert).
        /// @return View driven by the smallest pool; an empty view if any pool does not exist.
        template <Component... Components>
        QueryView<Components...> View() {
            static_assert(sizeof...(Components) > 0, "View requires at least one component");

            if (!(HasPool<Components>() && ...))
                return QueryView<Components...>(nullptr, {}, static_cast<ComponentPool<Components>*>(nullptr)...);

            const std::array<IComponentPool*, sizeof...(Components)> pools{&Pool<Components>()...};

            auto* smallest = pools.front();
            for (auto* pool : pools) {
                if (pool->Entities().size() < smallest->Entities().size())
                    smallest = pool;
            }

            return QueryView<Components...>(smallest, smallest->Entities(), &Pool<Components>()...);
        }

        /// @brief Removes an entity's components from every pool.
        /// @param entity Entity to strip. Pools not containing it are unaffected.
        /// @complexity O(number of pools).
        void RemoveEntity(Entity entity);

        /// @brief Empties every pool.
        /// @post No component remains; the pools themselves stay allocated.
        void Clear();

    private:
        std::vector<std::unique_ptr<IComponentPool>> m_pools;
    };
} // namespace Pumpkin::ECS
