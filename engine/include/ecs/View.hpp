#pragma once

#include "Registry.hpp"
#include <tuple>
#include <algorithm>

namespace Engine {

template<typename... Components> class View
{
  private:
    Registry& m_registry;

  public:
    View(Registry& registry) : m_registry(registry) {}

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
