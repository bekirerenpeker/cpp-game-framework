#pragma once

#include "Entity.hpp"
#include "ecs/signals/Signal.hpp"
#include "ecs/signals/Sink.hpp"
#include <vector>

namespace Engine {

class ISparseSet
{
  public:
    virtual ~ISparseSet() = default;
    virtual void remove(Entity e) = 0;
};

template<typename T> class SparseSet : ISparseSet
{
    std::vector<T> m_dense;
    std::vector<Entity> m_entities;
    std::vector<size_t> m_sparse;

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

    size_t size() const { return m_dense.size(); }
    size_t capacity() const { return m_sparse.size(); }

    void clear()
    {
        for (Entity e : m_entities) m_onDestroy.publish(e);
        m_dense.clear();
        m_entities.clear();
        m_sparse.clear();
    }

    bool contains(Entity e) const
    {
        size_t id = getEntityId(e);
        return e != NULL_ENTITY && id < m_sparse.size() && m_entities[m_sparse[id]] == e;
    }
    T& get(Entity e)
    {
        assert(contains(e) && "Entity doesn't have this component");
        return m_dense[m_sparse[getEntityId(e)]];
    }
    const T& get(Entity e) const
    {
        assert(contains(e) && "Entity doesn't have this component");
        return m_dense[m_sparse[getEntityId(e)]];
    }

    T& insert(Entity e, T&& comp)
    {
        uint32_t id = getEntityId(e);
        if (id >= m_sparse.size()) m_sparse.resize(id + 1, NULL_ENTITY);

        if (!contains(e)) {
            size_t idx = m_dense.size();
            m_sparse[id] = idx;
            m_dense.push_back(std::move(comp));
            m_entities.push_back(e);
            m_onCreate.publish(e);
            return m_dense[idx];
        } else {
            m_dense[m_sparse[id]] = std::move(comp);
            m_onSet.publish(e);
            return m_dense[m_sparse[id]];
        }
    }

    void remove(Entity e) override
    {
        assert(contains(e) && "Entity doesn't have this component");
        uint32_t id = getEntityId(e);
        size_t idx = m_sparse[e], last = m_dense.size() - 1;

        m_onDestroy.publish(e);

        if (idx != last) {
            m_dense[idx] = std::move(m_dense[last]);
            m_entities[idx] = m_entities[last];
            m_sparse[getEntityId(m_entities[idx])] = idx;
        }

        m_sparse[id] = NULL_ENTITY;
        m_dense.pop_back();
        m_entities.pop_back();
    }
};

}   // namespace Engine
