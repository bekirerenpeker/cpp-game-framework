#pragma once

#include "Entity.hpp"
#include "ecs/signals/Signal.hpp"
#include "ecs/signals/Sink.hpp"
#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

namespace Engine {

class ISparseSet
{
  public:
    virtual ~ISparseSet() = default;
    virtual void remove(Entity e) = 0;
    virtual void clone(Entity from, Entity to) = 0;
    virtual bool contains(Entity e) const = 0;
};

struct InsertionSort
{
    template<typename T, typename Compare>
    void operator()(
        std::vector<T>& dense, std::vector<Entity>& entities, std::vector<size_t>&, Compare compare
    ) const
    {
        for (size_t i = 1; i < dense.size(); i++) {
            size_t j = i;
            while (j > 0 && compare(dense[j], dense[j - 1])) {
                std::swap(dense[j], dense[j - 1]);
                std::swap(entities[j], entities[j - 1]);
                j--;
            }
        }
    }
};

struct StdSort
{
    template<typename T, typename Compare>
    void operator()(
        std::vector<T>& dense, std::vector<Entity>& entities, std::vector<size_t>& indices,
        Compare compare
    ) const
    {
        size_t count = dense.size();
        indices.resize(count);
        for (size_t i = 0; i < count; i++) indices[i] = i;

        std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
            return compare(dense[a], dense[b]);
        });

        for (size_t i = 0; i < count; i++) {
            size_t curr = i;
            while (i != indices[curr]) {
                size_t next = indices[curr];
                std::swap(dense[curr], dense[next]);
                std::swap(entities[curr], entities[next]);
                indices[curr] = curr;
                curr = next;
            }
            indices[curr] = curr;
        }
    }
};

template<typename T, bool isEmpty = std::is_empty_v<T>> class SparseSet : public ISparseSet
{
    std::vector<T> m_dense;
    std::vector<Entity> m_entities;
    std::vector<size_t> m_sparse;
    std::vector<size_t> m_sortIndices;

    Signal<void(Entity)> m_onCreate;
    Signal<void(Entity)> m_onSet;
    Signal<void(Entity)> m_onDestroy;

  public:
    SparseSet() = default;
    ~SparseSet() override = default;

    SparseSet(const SparseSet&) = delete;
    SparseSet& operator=(const SparseSet&) = delete;
    SparseSet(SparseSet&&) = default;
    SparseSet& operator=(SparseSet&&) = default;

    Sink<void(Entity)> onCreate() { return Sink<void(Entity)> {m_onCreate}; }
    Sink<void(Entity)> onDestroy() { return Sink<void(Entity)> {m_onDestroy}; }
    Sink<void(Entity)> onSet() { return Sink<void(Entity)> {m_onSet}; }

    const std::vector<Entity>& getEntities() const { return m_entities; }
    const std::vector<T>& getComponents() const { return m_dense; }
    std::vector<T>& getComponents() { return m_dense; }

    size_t size() const { return m_entities.size(); }
    size_t capacity() const { return m_sparse.size(); }

    void clear()
    {
        for (Entity e : m_entities) m_onDestroy.publish(e);
        if constexpr (!isEmpty) m_dense.clear();
        m_entities.clear();
        m_sparse.clear();
    }

    bool contains(Entity e) const override
    {
        size_t id = getEntityId(e);
        return e != NULL_ENTITY && id < m_sparse.size() && m_sparse[id] != NULL_ENTITY &&
               m_entities[m_sparse[id]] == e;
    }
    T& get(Entity e)
    {
        assert(contains(e) && "Entity doesn't have this component");
        if constexpr (isEmpty) {
            static T dummy {};
            return dummy;
        } else {
            return m_dense[m_sparse[getEntityId(e)]];
        }
    }
    const T& get(Entity e) const
    {
        assert(contains(e) && "Entity doesn't have this component");
        if constexpr (isEmpty) {
            static T dummy {};
            return dummy;
        } else {
            return m_dense[m_sparse[getEntityId(e)]];
        }
    }

    T& insert(Entity e, T&& comp)
    {
        uint32_t id = getEntityId(e);
        if (id >= m_sparse.size()) m_sparse.resize(id + 1, NULL_ENTITY);

        if (!contains(e)) {
            size_t idx = m_entities.size();
            m_sparse[id] = idx;
            if constexpr (!isEmpty) m_dense.push_back(std::move(comp));
            m_entities.push_back(e);
            m_onCreate.publish(e);
            if constexpr (isEmpty) {
                static T dummy {};
                return dummy;
            } else {
                return m_dense[idx];
            }
        } else {
            if constexpr (!isEmpty) { m_dense[m_sparse[id]] = std::move(comp); }
            m_onSet.publish(e);
            if constexpr (isEmpty) {
                static T dummy {};
                return dummy;
            } else {
                return m_dense[m_sparse[id]];
            }
        }
    }

    void remove(Entity e) override
    {
        assert(contains(e) && "Entity doesn't have this component");
        uint32_t id = getEntityId(e);
        size_t idx = m_sparse[id], last = m_entities.size() - 1;

        m_onDestroy.publish(e);

        if (idx != last) {
            if constexpr (!isEmpty) m_dense[idx] = std::move(m_dense[last]);
            m_entities[idx] = m_entities[last];
            m_sparse[getEntityId(m_entities[idx])] = idx;
        }

        m_sparse[id] = NULL_ENTITY;
        if constexpr (!isEmpty) { m_dense.pop_back(); }
        m_entities.pop_back();
    }

    void clone(Entity from, Entity to) override
    {
        if (contains(from)) {
            if constexpr (isEmpty) insert(to, T {});
            else insert(to, T(get(from)));
        }
    }

    template<typename Compare, typename Algo = InsertionSort>
    void sort(Compare compare, Algo algo = Algo {})
    {
        if constexpr (isEmpty) {
            return;
        } else {
            if (m_entities.size() < 2) return;

            algo(m_dense, m_entities, m_sortIndices, compare);
            for (size_t i = 0; i < m_entities.size(); i++) {
                m_sparse[getEntityId(m_entities[i])] = i;
            }
        }
    }
};

}   // namespace Engine
