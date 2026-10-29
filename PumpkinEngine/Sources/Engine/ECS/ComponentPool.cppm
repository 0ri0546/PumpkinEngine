/// @file ComponentPool.cppm
/// @brief Module Pumpkin.ECS.ComponentPool: type-erased and typed per-component storage.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.ComponentPool;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.SparseSet;

import std;

export namespace Pumpkin::ECS {
    /// @brief Type-erased interface of a component pool.
    ///
    /// Lets the Registry operate on every pool (remove an entity everywhere, clear, query the
    /// smallest pool) without knowing the component types.
    ///
    /// @thread_safety Same rules as SparseSet: no concurrent structural changes.
    class PUMPKIN_API IComponentPool {
    public:
        /// @brief Virtual destructor, pools are owned through `std::unique_ptr<IComponentPool>`.
        virtual ~IComponentPool() = default;

        /// @brief Removes the component of @p entity, if present.
        /// @param entity Entity to strip from this pool. No-op if absent.
        virtual void Remove(Entity entity) = 0;
        /// @brief Tests whether @p entity has a component in this pool.
        /// @param entity Handle to test (generation-checked).
        /// @return true if present.
        virtual bool Contains(Entity entity) const = 0;
        /// @brief Pre-allocates storage.
        /// @param capacity Number of components to reserve room for.
        virtual void Reserve(std::size_t capacity) = 0;
        /// @brief Removes every component from the pool.
        virtual void Clear() = 0;
        /// @brief Entities that own a component in this pool.
        /// @return Dense span of owners (order not stable across removals).
        /// @warning Invalidated by structural changes.
        virtual std::span<const Entity> Entities() const = 0;
    };

    /// @brief Storage for all components of type @p T, backed by a SparseSet.
    ///
    /// @tparam T Component type.
    /// @thread_safety Concurrent mutation of different entities' components is safe provided
    ///                nobody changes the pool structure (Emplace/Remove/Clear/Reserve); this is
    ///                the guarantee the Scheduler relies on.
    template <Component T>
    class PUMPKIN_API ComponentPool final : public IComponentPool {
    public:
        /// @brief Adds a component for @p entity, constructing it in place.
        /// @tparam Args Constructor argument types.
        /// @param entity Owner of the new component.
        /// @param args   Arguments forwarded to T's constructor.
        /// @return Reference to the component. If the entity already had one, it is returned unchanged
        ///         and @p args are ignored.
        /// @see SparseSet::Emplace
        template <typename... Args>
        T& Emplace(Entity entity, Args&&... args) {
            return m_storage.Emplace(entity, std::forward<Args>(args)...);
        }

        /// @copydoc IComponentPool::Remove
        void Remove(Entity entity) override {
            m_storage.Remove(entity);
        }

        /// @copydoc IComponentPool::Contains
        bool Contains(Entity entity) const override {
            return m_storage.Contains(entity);
        }

        /// @copydoc IComponentPool::Reserve
        void Reserve(std::size_t capacity) override {
            m_storage.Reserve(capacity);
        }

        /// @copydoc IComponentPool::Clear
        void Clear() override {
            m_storage.Clear();
        }

        /// @brief Checked access to the component of @p entity.
        /// @param entity Owner.
        /// @return Mutable reference to the component.
        /// @throws std::out_of_range if @p entity has no such component.
        T& Get(Entity entity) {
            return m_storage.Get(entity);
        }

        /// @brief Checked read-only access to the component of @p entity.
        /// @param entity Owner.
        /// @return Const reference to the component.
        /// @throws std::out_of_range if @p entity has no such component.
        const T& Get(Entity entity) const {
            return m_storage.Get(entity);
        }

        /// @brief Unchecked access to the component of @p entity.
        /// @param entity Owner.
        /// @pre Contains(entity) is true (undefined behaviour otherwise).
        /// @return Mutable reference to the component.
        T& Fetch(Entity entity) {
            return m_storage.Fetch(entity);
        }

        /// @brief Non-throwing lookup.
        /// @param entity Owner.
        /// @return Pointer to the component or nullptr.
        T* TryGet(Entity entity) {
            return m_storage.TryGet(entity);
        }

        /// @brief Non-throwing read-only lookup.
        /// @param entity Owner.
        /// @return Const pointer to the component or nullptr.
        const T* TryGet(Entity entity) const {
            return m_storage.TryGet(entity);
        }

        /// @copydoc IComponentPool::Entities
        std::span<const Entity> Entities() const override {
            return m_storage.Entities();
        }

        /// @brief Mutable view over all components, index-aligned with Entities().
        /// @warning Invalidated by structural changes.
        std::span<T> Values() {
            return m_storage.Values();
        }

        /// @brief Read-only view over all components, index-aligned with Entities().
        /// @warning Invalidated by structural changes.
        std::span<const T> Values() const {
            return m_storage.Values();
        }

    private:
        SparseSet<T> m_storage;
    };
} // namespace Pumpkin::ECS
