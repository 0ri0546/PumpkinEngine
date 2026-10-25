export module Pumpkin.ECS.ComponentPool;

import Pumpkin.ECS.Component;
import Pumpkin.ECS.Entity;
import Pumpkin.ECS.SparseSet;

import std;

export namespace Pumpkin::ECS {
    class IComponentPool {
    public:
        virtual ~IComponentPool() = default;

        virtual void Remove(Entity entity) = 0;
        virtual bool Contains(Entity entity) const = 0;
        virtual void Reserve(std::size_t capacity) = 0;
        virtual void Clear() = 0;
        virtual std::span<const Entity> Entities() const = 0;
    };

    template <Component T>
    class ComponentPool final : public IComponentPool {
    public:
        template <typename... Args>
        T& Emplace(Entity entity, Args&&... args) {
            return m_storage.Emplace(entity, std::forward<Args>(args)...);
        }

        void Remove(Entity entity) override {
            m_storage.Remove(entity);
        }

        bool Contains(Entity entity) const override {
            return m_storage.Contains(entity);
        }

        void Reserve(std::size_t capacity) override {
            m_storage.Reserve(capacity);
        }

        void Clear() override {
            m_storage.Clear();
        }

        T& Get(Entity entity) {
            return m_storage.Get(entity);
        }

        const T& Get(Entity entity) const {
            return m_storage.Get(entity);
        }

        T* TryGet(Entity entity) {
            return m_storage.TryGet(entity);
        }

        const T* TryGet(Entity entity) const {
            return m_storage.TryGet(entity);
        }

        std::span<const Entity> Entities() const override {
            return m_storage.Entities();
        }

        std::span<T> Values() {
            return m_storage.Values();
        }

        std::span<const T> Values() const {
            return m_storage.Values();
        }

    private:
        SparseSet<T> m_storage;
    };
} // namespace Pumpkin::ECS
