/// @file SparseSet.cppm
/// @brief Module Pumpkin.ECS.SparseSet: dense component storage indexed by Entity.

module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.SparseSet;

import Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    /// @brief Sparse-set container mapping entities to values of type @p T.
    ///
    /// Values live contiguously in a dense array (cache-friendly iteration); a sparse array
    /// indexed by Entity::index stores `denseIndex + 1` (0 means "absent"). A parallel dense
    /// array holds the owning Entity of each value, so Entities() and Values() are index-aligned.
    ///
    /// @tparam T Stored value type (move-constructible and move-assignable).
    /// @note Insertion order is not preserved after a Remove() (swap-and-pop).
    /// @warning Any structural change (Emplace, Remove, Clear) may invalidate references,
    ///          pointers and spans previously obtained from the set.
    /// @thread_safety Not thread-safe for structural changes. Concurrent const access is safe,
    ///                and so is concurrent mutation of *different* values through Get/Fetch/
    ///                TryGet as long as no thread changes the structure.
    template <typename T>
    class PUMPKIN_API SparseSet {
    public:
        /// @brief Creates an empty set.
        SparseSet() = default;
        /// @brief Non-copyable (values are owned uniquely).
        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;
        /// @brief Move-constructs, taking over the storage of the source.
        SparseSet(SparseSet&&) noexcept = default;
        SparseSet& operator=(SparseSet&&) noexcept = default;

        /// @brief Inserts a value for @p entity, constructing it in place.
        ///
        /// Behaviour by case:
        /// - Entity absent: the value is appended to the dense array.
        /// - Entity already present (same index and generation): nothing is constructed or assigned,
        ///   the existing value is returned and @p args are ignored.
        /// - Sparse slot used by a *stale* handle (same index, older generation): that entry is
        ///   overwritten in place with `T(args...)`.
        ///
        /// @tparam Args Constructor argument types.
        /// @param entity Owner of the value.
        /// @param args   Arguments forwarded to T's constructor.
        /// @return Reference to the stored value (new or pre-existing).
        /// @complexity Amortised O(1).
        /// @warning The returned reference is invalidated by the next structural change.
        template <typename... Args>
        T& Emplace(Entity entity, Args&&... args) {
            if (entity.index >= m_sparse.size()) [[unlikely]] {
                const std::size_t new_size = std::max<std::size_t>(entity.index + 1, m_sparse.size() * 2);
                m_sparse.resize(new_size, 0);
            }

            auto& sparse_slot = m_sparse[entity.index];
            if (sparse_slot != 0) [[unlikely]] {
                const auto dense_index = sparse_slot - 1;
                if (m_denseEntities[dense_index] == entity)
                    return m_dense[dense_index]; // already present

                m_denseEntities[dense_index] = entity;
                m_dense[dense_index] = T(std::forward<Args>(args)...);
                return m_dense[dense_index];
            }

            const auto index = m_dense.size();
            m_denseEntities.push_back(entity);
            m_dense.emplace_back(std::forward<Args>(args)...);

            sparse_slot = index + 1;

            return m_dense.back();
        }

        /// @brief Removes the value owned by @p entity, if any.
        ///
        /// The last dense element is moved into the freed position (swap-and-pop) and its sparse
        /// slot is patched, so removal is O(1) but reorders the dense arrays.
        ///
        /// @param entity Entity whose value is removed. No-op if absent (or if the handle is stale).
        /// @complexity O(1).
        void Remove(Entity entity) {
            if (!Contains(entity))
                return;

            const auto index = m_sparse[entity.index] - 1;
            const auto last = m_dense.size() - 1;

            if (index != last) {
                m_denseEntities[index] = m_denseEntities[last];

                m_dense[index] = std::move(m_dense[last]);

                const Entity movedEntity = m_denseEntities[index];

                m_sparse[movedEntity.index] = index + 1;
            }

            m_denseEntities.pop_back();
            m_dense.pop_back();

            m_sparse[entity.index] = 0;
        }

        /// @brief Pre-allocates capacity for @p capacity elements.
        ///
        /// @param capacity Number of values to reserve in the dense arrays; also reserved in the
        ///                 sparse array, which is indexed by Entity::index and still grows on demand.
        void Reserve(std::size_t capacity) {
            m_sparse.reserve(capacity);
            m_dense.reserve(capacity);
            m_denseEntities.reserve(capacity);
        }

        /// @brief Tests whether @p entity currently owns a value.
        /// @param entity Handle to test; the generation must match, so stale handles return false.
        /// @return true if a value for exactly this handle is stored.
        /// @complexity O(1).
        bool Contains(Entity entity) const {
            if (entity.index >= m_sparse.size())
                return false;

            const auto slot = m_sparse[entity.index];

            return slot != 0 && m_denseEntities[slot - 1] == entity;
        }

        /// @brief Checked access to the value of @p entity.
        /// @param entity Owner of the value.
        /// @return Mutable reference to the value.
        /// @throws std::out_of_range if the entity has no value in this set.
        /// @see Fetch, TryGet
        T& Get(Entity entity) {
            if (!Contains(entity))
                throw std::out_of_range("SparseSet::Get: entity not present");

            return m_dense[m_sparse[entity.index] - 1];
        }

        /// @brief Checked read-only access to the value of @p entity.
        /// @param entity Owner of the value.
        /// @return Const reference to the value.
        /// @throws std::out_of_range if the entity has no value in this set.
        const T& Get(Entity entity) const {
            if (!Contains(entity))
                throw std::out_of_range("SparseSet::Get: entity not present");

            return m_dense[m_sparse[entity.index] - 1];
        }

        /// @brief Unchecked access to the value of @p entity (hot-path variant of Get()).
        /// @param entity Owner of the value.
        /// @pre Contains(entity) is true; otherwise the behaviour is undefined.
        /// @return Mutable reference to the value.
        // Unchecked access: the caller guarantees Contains(entity)
        T& Fetch(Entity entity) {
            return m_dense[m_sparse[entity.index] - 1];
        }

        /// @brief Non-throwing lookup.
        /// @param entity Owner of the value.
        /// @return Pointer to the value, or nullptr if absent.
        T* TryGet(Entity entity) {
            if (!Contains(entity))
                return nullptr;

            return &m_dense[m_sparse[entity.index] - 1];
        }

        /// @brief Non-throwing read-only lookup.
        /// @param entity Owner of the value.
        /// @return Const pointer to the value, or nullptr if absent.
        const T* TryGet(Entity entity) const {
            if (!Contains(entity))
                return nullptr;

            return &m_dense[m_sparse[entity.index] - 1];
        }

        /// @brief Entities that own a value, in dense order.
        /// @return Span index-aligned with Values(): `Entities()[i]` owns `Values()[i]`.
        /// @warning Invalidated by structural changes.
        std::span<const Entity> Entities() const {
            return m_denseEntities;
        }

        /// @brief Mutable view over all stored values (dense order).
        /// @return Span index-aligned with Entities().
        /// @warning Invalidated by structural changes.
        std::span<T> Values() {
            return m_dense;
        }

        /// @brief Read-only view over all stored values (dense order).
        /// @return Const span index-aligned with Entities().
        std::span<const T> Values() const {
            return m_dense;
        }

        /// @brief Removes every value.
        /// @post Contains() is false for every entity; size is 0. Allocated capacity is kept.
        void Clear() {
            m_sparse.clear();
            m_dense.clear();
            m_denseEntities.clear();
        }

    private:
        std::vector<Entity> m_denseEntities;
        std::vector<T> m_dense;
        std::vector<std::uint32_t> m_sparse;
    };
} // namespace Pumpkin::ECS
