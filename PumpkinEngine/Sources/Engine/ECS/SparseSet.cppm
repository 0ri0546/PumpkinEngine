module;

#include <DLL/Export.hpp>

export module Pumpkin.ECS.SparseSet;

import Pumpkin.ECS.Entity;

import std;

export namespace Pumpkin::ECS {
    template <typename T>
    class PUMPKIN_API SparseSet {
    public:
        SparseSet() = default;
        SparseSet(const SparseSet&) = delete;
        SparseSet& operator=(const SparseSet&) = delete;
        SparseSet(SparseSet&&) noexcept = default;
        SparseSet& operator=(SparseSet&&) noexcept = default;

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

        void Reserve(std::size_t capacity) {
            m_sparse.reserve(capacity);
            m_dense.reserve(capacity);
            m_denseEntities.reserve(capacity);
        }

        bool Contains(Entity entity) const {
            if (entity.index >= m_sparse.size())
                return false;

            const auto slot = m_sparse[entity.index];

            return slot != 0 && m_denseEntities[slot - 1] == entity;
        }

        T& Get(Entity entity) {
            if (!Contains(entity))
                throw std::out_of_range("SparseSet::Get: entity not present");

            return m_dense[m_sparse[entity.index] - 1];
        }

        const T& Get(Entity entity) const {
            if (!Contains(entity))
                throw std::out_of_range("SparseSet::Get: entity not present");

            return m_dense[m_sparse[entity.index] - 1];
        }

        // Unchecked access: the caller guarantees Contains(entity)
        T& Fetch(Entity entity) {
            return m_dense[m_sparse[entity.index] - 1];
        }

        T* TryGet(Entity entity) {
            if (!Contains(entity))
                return nullptr;

            return &m_dense[m_sparse[entity.index] - 1];
        }

        const T* TryGet(Entity entity) const {
            if (!Contains(entity))
                return nullptr;

            return &m_dense[m_sparse[entity.index] - 1];
        }

        std::span<const Entity> Entities() const {
            return m_denseEntities;
        }

        std::span<T> Values() {
            return m_dense;
        }

        std::span<const T> Values() const {
            return m_dense;
        }

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
