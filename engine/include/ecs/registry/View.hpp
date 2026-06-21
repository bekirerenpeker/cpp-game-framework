#pragma once

#include "Registry.hpp"
#include <tuple>
#include <type_traits>

namespace Engine {

// Tag used to specify components that should be excluded from the view
template<typename... Types> struct Exclude
{
};

namespace detail {
// ---------------------------------------------------------------------------------
// Metaprogramming helpers to extract `Exclude<...>` from variadic template arguments
// so that the user can seamlessly write `View<Position, Velocity, Exclude<Static>>`
// ---------------------------------------------------------------------------------

template<typename T> struct is_exclude : std::false_type
{
};
template<typename... Ts> struct is_exclude<Exclude<Ts...>> : std::true_type
{
};

// Extract Exclude<...>
template<typename... Args> struct get_exclude
{
    using type = Exclude<>;
};
template<typename T, typename... Rest> struct get_exclude<T, Rest...>
{
    using type = std::conditional_t<is_exclude<T>::value, T, typename get_exclude<Rest...>::type>;
};

// Extract Includes (Everything that is not Exclude<...>)
template<typename... Ts> struct type_list
{
};

template<typename List, typename... Args> struct get_includes;

template<typename... Includes> struct get_includes<type_list<Includes...>>
{
    using type = std::tuple<Includes...>;
};

template<typename... Includes, typename T, typename... Rest>
struct get_includes<type_list<Includes...>, T, Rest...>
{
    using type = std::conditional_t<
        is_exclude<T>::value, typename get_includes<type_list<Includes...>, Rest...>::type,
        typename get_includes<type_list<Includes..., T>, Rest...>::type>;
};
}   // namespace detail

template<typename Includes, typename Excludes> class ViewImpl;

template<typename... Includes, typename... Excludes>
class ViewImpl<std::tuple<Includes...>, Exclude<Excludes...>>
{
  private:
    Registry& m_registry;

    // Dynamically finds the smallest pool to act as the driving iterator array
    const std::vector<Entity>& getDrivingEntities()
    {
        size_t minSize = static_cast<size_t>(-1);
        const std::vector<Entity>* drivingPool = nullptr;

        auto checkPool = [&](auto* pool) {
            if (pool->size() < minSize) {
                minSize = pool->size();
                drivingPool = &pool->getEntities();
            }
        };

        // Fold expression evaluates shortest pool across all include types
        (checkPool(&m_registry.getPool<Includes>()), ...);

        return *drivingPool;
    }

    class Iterator
    {
        Registry& reg;
        size_t index;
        const std::vector<Entity>& entities;

      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::tuple<Entity, Includes&...>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        Iterator(Registry& r, size_t idx, const std::vector<Entity>& ents)
            : reg(r), index(idx), entities(ents)
        {
            // Advance to first matching entity
            if (index < entities.size() && !match(entities[index])) { operator++(); }
        }

        bool match(Entity e) const
        {
            bool hasIncludes = ((reg.getPool<Includes>().contains(e)) && ... && true);
            bool hasExcludes = ((reg.getPool<Excludes>().contains(e)) || ... || false);
            return hasIncludes && !hasExcludes;
        }

        bool operator!=(const Iterator& other) const { return index != other.index; }

        Iterator& operator++()
        {
            do {
                index++;
            } while (index < entities.size() && !match(entities[index]));
            return *this;
        }

        auto operator*() const
        {
            Entity e = entities[index];
            return std::tie(e, reg.getPool<Includes>().get(e)...);
        }
    };

  public:
    ViewImpl(Registry& registry) : m_registry(registry) {}

    auto begin()
    {
        const auto& entities = getDrivingEntities();
        return Iterator(m_registry, 0, entities);
    }

    auto end()
    {
        const auto& entities = getDrivingEntities();
        return Iterator(m_registry, entities.size(), entities);
    }

    template<typename Func> void each(Func&& func)
    {
        // 1. Hyper-optimized path for single-type views without exclusions
        if constexpr (sizeof...(Includes) == 1 && sizeof...(Excludes) == 0) {
            using T = std::tuple_element_t<0, std::tuple<Includes...>>;
            auto& pool = m_registry.getPool<T>();
            for (Entity e : pool.getEntities()) { func(e, pool.get(e)); }
        }
        // 2. Multi-type or excluded-type view
        else {
            const auto& drivingEntities = getDrivingEntities();
            for (Entity e : drivingEntities) {
                bool hasIncludes = ((m_registry.getPool<Includes>().contains(e)) && ... && true);
                bool hasExcludes = ((m_registry.getPool<Excludes>().contains(e)) || ... || false);

                if (hasIncludes && !hasExcludes) {
                    func(e, m_registry.getPool<Includes>().get(e)...);
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------------
// Primary View class wrapper.
// This allows the user to write: `View<Position, Velocity, Exclude<Static>>`
// seamlessly, and handles type extraction automatically.
// ---------------------------------------------------------------------------------
template<typename... Args>
class View :
    public ViewImpl<
        typename detail::get_includes<detail::type_list<>, Args...>::type,
        typename detail::get_exclude<Args...>::type>
{
  public:
    using Base = ViewImpl<
        typename detail::get_includes<detail::type_list<>, Args...>::type,
        typename detail::get_exclude<Args...>::type>;

    View(Registry& registry) : Base(registry) {}
};

}   // namespace Engine
