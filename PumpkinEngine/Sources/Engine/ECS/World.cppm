/// @file World.cppm
/// @brief Module Pumpkin.ECS.World: entity lifetime management and component access facade.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.World;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.Registry;

import std;

export namespace Pumpkin::ECS {
    /// @brief Owns entities and their components.
    ///
    /// World allocates generational Entity handles (recycling freed indices) and forwards
    /// component operations to its Registry. Most operations are *structural* (they can
    /// allocate or move storage) and must not run while systems execute in parallel: use
    /// CommandBuffer from systems.
    ///
    /// @thread_safety Not thread-safe. Read-only calls (IsAlive, Has, Get const, TryGet const)
    ///                may run concurrently with each other as long as nobody mutates the World.
    /// @see Registry, CommandBuffer, Scheduler
    class PUMPKIN_API World {
    public:
        /// @brief Creates a new entity.
        ///
        /// Reuses the most recently freed index if there is one (LIFO), otherwise takes the next
        /// unused index; the slot arrays grow geometrically (at least 1024 slots).
        ///
        /// @return Handle of the new, alive entity (index >= 1).
        /// @complexity Amortised O(1).
        Entity CreateEntity();
        /// @brief Destroys an entity and all of its components.
        ///
        /// Removes the entity from every pool, marks the slot dead, bumps its generation (so every
        /// existing copy of the handle becomes stale) and recycles the index.
        ///
        /// @param entity Entity to destroy. No-op if it is not alive.
        /// @complexity O(number of component pools).
        void DestroyEntity(Entity entity);

        /// @brief Tests whether a handle refers to a live entity.
        /// @param entity Handle to test.
        /// @return false for NullEntity, out-of-range indices, stale generations and destroyed entities.
        /// @complexity O(1).
        bool IsAlive(Entity entity) const;

        /// @brief Destroys every entity and every component.
        /// @post All previously issued handles are invalid; all indices are available again and the
        ///       lowest index is reused first. Pools stay allocated (empty).
        /// @warning Invalidates references/spans into component storage.
        void Clear();

        /// @brief Pre-allocates component storage.
        ///
        /// Creates the pool of each listed component if it does not exist yet and reserves room in it.
        ///
        /// @tparam Components Component types to reserve for.
        /// @param capacity Number of components per pool.
        /// @note Does not reserve entity slots.
        template <typename... Components>
        void Reserve(std::size_t capacity) {
            (m_registry.Pool<Components>().Reserve(capacity), ...);
        }

        /// @brief Attaches a component to an entity (constructed in place).
        ///
        /// The pool of @p T is created on first use. If the entity already has a T, that component
        /// is returned untouched and @p args are ignored.
        ///
        /// @tparam T    Component type.
        /// @tparam Args Constructor argument types.
        /// @param entity Target entity.
        /// @param args   Arguments forwarded to T's constructor.
        /// @return Reference to the component.
        /// @throws std::invalid_argument if @p entity is not alive.
        /// @warning Structural: not allowed while systems run, record it through CommandBuffer::Add.
        template <Component T, typename... Args>
        T& Add(Entity entity, Args&&... args) {
            if (!IsAlive(entity)) [[unlikely]]
                throw std::invalid_argument("World::Add: entity is not alive");

            return m_registry.Pool<T>().Emplace(entity, std::forward<Args>(args)...);
        }

        /// @brief Detaches the component T from an entity.
        /// @tparam T Component type.
        /// @param entity Target entity. No-op if it has no T.
        /// @note Creates the (empty) pool of T if it did not exist.
        /// @warning Structural: use CommandBuffer::Remove from systems.
        template <Component T>
        void Remove(Entity entity) {
            m_registry.Pool<T>().Remove(entity);
        }

        /// @brief Tests whether an entity has a component T.
        /// @tparam T Component type.
        /// @param entity Entity to test.
        /// @return true if present; false if absent or if no pool for T exists.
        template <Component T>
        bool Has(Entity entity) const {
            const auto* pool = m_registry.TryPool<T>();
            return pool && pool->Contains(entity);
        }

        /// @brief Checked access to a component.
        /// @tparam T Component type.
        /// @param entity Owner.
        /// @return Mutable reference to the component.
        /// @throws std::out_of_range if @p entity has no T.
        /// @note Creates the (empty) pool of T if it did not exist.
        /// @see TryGet
        template <Component T>
        T& Get(Entity entity) {
            return m_registry.Pool<T>().Get(entity);
        }

        /// @brief Checked read-only access to a component.
        /// @tparam T Component type.
        /// @param entity Owner.
        /// @return Const reference to the component.
        /// @throws std::out_of_range if @p entity has no T, or if T's id lies outside the pool table.
        /// @warning If the pool table already covers T's id but T's pool was never created, this
        ///          overload dereferences an empty pointer (undefined behaviour). Use TryGet() or
        ///          Has() when the component may never have been added.
        template <Component T>
        const T& Get(Entity entity) const {
            return m_registry.Pool<T>().Get(entity);
        }

        /// @brief Non-throwing component lookup.
        /// @tparam T Component type.
        /// @param entity Owner.
        /// @return Pointer to the component, or nullptr if absent / no pool.
        template <Component T>
        T* TryGet(Entity entity) {
            auto* pool = m_registry.TryPool<T>();
            return pool ? pool->TryGet(entity) : nullptr;
        }

        /// @brief Non-throwing read-only component lookup.
        /// @tparam T Component type.
        /// @param entity Owner.
        /// @return Const pointer to the component, or nullptr if absent / no pool.
        template <Component T>
        const T* TryGet(Entity entity) const {
            const auto* pool = m_registry.TryPool<T>();
            return pool ? pool->TryGet(entity) : nullptr;
        }

        /// @brief Collects the entities that have *all* the listed components.
        ///
        /// @tparam Components Required component types (distinct).
        /// @return Snapshot vector of matching entities; empty when the pack is empty or any pool is missing.
        /// @complexity O(n_min * k): iterates the smallest pool and probes the others.
        /// @see View for allocation-free iteration with component references.
        template <Component... Components>
        std::vector<Entity> Query() {
            return m_registry.Query<Components...>();
        }

        /// @brief Collects the entities that have *at least one* of the listed components.
        ///
        /// @tparam Components Component types (any of them qualifies).
        /// @return Snapshot vector without duplicates, ordered by pool then dense order; missing pools are skipped.
        template <Component... Components>
        std::vector<Entity> QueryAny() {
            return m_registry.QueryAny<Components...>();
        }

        /// @brief Returns a lightweight, non-owning view over entities having all listed components.
        /// @tparam Components Required component types (distinct, at least one).
        /// @return View yielding `std::tuple<Entity, Components&...>`; empty if any pool is missing.
        /// @warning The view is invalidated by structural changes to any involved pool.
        /// @see Registry::QueryView
        template <Component... Components>
        Registry::QueryView<Components...> View() {
            return m_registry.View<Components...>();
        }

        /// @brief Access to the underlying component registry.
        /// @return Mutable reference to the Registry (used by the Scheduler).
        Registry& GetRegistry() { return m_registry; }

    private:
        std::uint32_t m_nextIndex = 1;
        std::vector<std::uint32_t> m_freeIndices;
        std::vector<std::uint32_t> m_generations;
        std::vector<std::uint8_t> m_alive;
        Registry m_registry;
    };
} // namespace Pumpkin::ECS
