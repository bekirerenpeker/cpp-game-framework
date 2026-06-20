#pragma once

#include "Registry.hpp"
#include <tuple>
#include <algorithm>

namespace Engine {

// TODO: improve the view class:
// add view chaining (combined = view1 | view2)
// currently we cant add a view function to registry since it leads to dependency injection but adding it allows for single line execution
// optimize the single type views (just use registry.getPool())
// optimize the multi type views (make the smallest pool the driving pool)
// add ability to specify excluded types
template<typename... Components> class View
{
  private:
    Registry& m_registry;

    class Iterator
    {
        Registry& reg;
        size_t index;
        const std::vector<Entity>& entities;

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::tuple<Entity, Components&...>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        Iterator(Registry& r, size_t idx, const std::vector<Entity>& ents)
            : reg(r), index(idx), entities(ents)
        {
            // If the current element doesn't match, skip to the next valid one
            if (index < entities.size() && !match(entities[index])) { operator++(); }
        }

        bool match(Entity e) const { return (reg.getPool<Components>().contains(e) && ...); }

        bool operator!=(const Iterator& other) const { return index != other.index; }

        Iterator& operator++()
        {
            do {
                index++;
            } while (index < entities.size() && !match(entities[index]));
            return *this;
        }

        // Returns a tuple of references for structured binding: auto [e, c1, c2] = it;
        auto operator*() const
        {
            Entity e = entities[index];
            return std::tie(e, reg.getPool<Components>().get(e)...);
        }
    };

  public:
    View(Registry& registry) : m_registry(registry) {}

    auto begin()
    {
        auto& pool =
            m_registry.getPool<typename std::tuple_element<0, std::tuple<Components...>>::type>();
        return Iterator(m_registry, 0, pool.getEntities());
    }

    auto end()
    {
        auto& pool =
            m_registry.getPool<typename std::tuple_element<0, std::tuple<Components...>>::type>();
        return Iterator(m_registry, pool.getEntities().size(), pool.getEntities());
    }

    template<typename Func> void each(Func&& func)
    {
        // 1. Get all pools as a tuple
        auto pools = std::forward_as_tuple(m_registry.getPool<Components>()...);

        // 2. Find the pool with the minimum size to act as our driver
        size_t sizes[] = {m_registry.getPool<Components>().size()...};
        size_t minIdx =
            std::distance(std::begin(sizes), std::min_element(std::begin(sizes), std::end(sizes)));

        // 3. Helper lambda to retrieve a pool by index from the tuple
        auto getPoolByIndex = [&](auto i) { return std::get<i.value>(pools); };

        // 4. Use the smallest pool as the driving pool
        switch (minIdx) {
        case 0: iterate<0>(func); break;
        case 1: iterate<1>(func); break;
        // ... for a small number of components this is fine,
        // but for a generic variadic solution, we use a visitor:
        default: iterate_any(minIdx, func); break;
        }
    }

  private:
    // This iterates using a specific component pool as the driver
    template<size_t DriverIdx, typename Func> void iterate(Func&& func)
    {
        auto& drivingPool =
            std::get<DriverIdx>(std::forward_as_tuple(m_registry.getPool<Components>()...));
        for (Entity e : drivingPool.getEntities()) {
            if ((m_registry.getPool<Components>().contains(e) && ...)) {
                func(e, m_registry.getPool<Components>().get(e)...);
            }
        }
    }

    // Generic visitor for variadic indices
    template<typename Func> void iterate_any(size_t index, Func&& func)
    {
        auto expand = [&](auto... i) { ((index == i ? iterate<i>(func) : (void)0), ...); };
        expand(std::make_index_sequence<sizeof...(Components)> {});
    }
};

}   // namespace Engine
