#pragma once

#include "utils/TypeAliases.hpp"
#include <vector>
#include <unordered_map>
#include <type_traits>
#include <utility>

namespace Engine {

class IHasId
{
  protected:
    IdType m_id;

  public:
    virtual ~IHasId() = default;

    virtual void setId(IdType id) { m_id = id; }
    virtual IdType getId() const { return m_id; }
};

template<
    typename T, bool inheritsId = std::is_base_of_v<IHasId, T>,
    bool isPointer = std::is_pointer_v<T>>
class IdIndexedVector
{
  private:
    IdType m_nextId = START_ID;
    std::unordered_map<IdType, size_t> m_idToIndex;
    std::vector<std::pair<IdType, T>> m_data;

  public:
    IdIndexedVector() = default;
    ~IdIndexedVector() = default;

    IdIndexedVector(const IdIndexedVector& other) = default;
    IdIndexedVector& operator=(const IdIndexedVector& other) = default;

    IdIndexedVector(IdIndexedVector&& other) noexcept
        : m_nextId(other.m_nextId),
          m_data(std::move(other.m_data)),
          m_idToIndex(std::move(other.m_idToIndex))
    {
    }
    IdIndexedVector& operator=(IdIndexedVector&& other) noexcept
    {
        m_nextId = other.m_nextId;
        m_data = std::move(other.m_data);
        m_idToIndex = std::move(other.m_idToIndex);
        return *this;
    }

    size_t size() const { return m_data.size(); }
    bool isEmpty() const { return m_data.empty(); }
    void clear()
    {
        m_nextId = START_ID;
        m_data.clear();
        m_idToIndex.clear();
    }

    std::vector<std::pair<IdType, T>>& getAll() { return m_data; }
    const std::vector<std::pair<IdType, T>>& getAll() const { return m_data; }

    template<typename... Args> IdType add(Args&&... args)
    {
        IdType id = m_nextId++;
        m_idToIndex[id] = m_data.size();

        // safe against objects that have deleted copy constructors
        m_data.emplace_back(
            std::piecewise_construct, std::forward_as_tuple(id),
            std::forward_as_tuple(std::forward<Args>(args)...)
        );

        if constexpr (inheritsId) {
            if constexpr (isPointer) {
                if (m_data.back().second) m_data.back().second->setId(id);
            } else {
                m_data.back().second.setId(id);
            }
        }

        return id;
    }

    template<typename U> bool set(IdType id, U&& value)
    {
        auto it = m_idToIndex.find(id);
        if (it == m_idToIndex.end()) return false;

        m_data[it->second].second = std::forward<U>(value);

        if constexpr (inheritsId) {
            if constexpr (isPointer) {
                if (m_data[it->second].second) m_data[it->second].second->setId(id);
            } else {
                m_data[it->second].second.setId(id);
            }
        }
        return true;
    }

    void remove(IdType id)
    {
        auto it = m_idToIndex.find(id);
        if (it == m_idToIndex.end()) return;

        size_t indexToRemove = it->second;
        size_t lastIndex = m_data.size() - 1;

        if (indexToRemove != lastIndex) {
            IdType lastElementId = m_data.back().first;
            m_data[indexToRemove] = std::move(m_data.back());
            m_idToIndex[lastElementId] = indexToRemove;
        }

        m_idToIndex.erase(it);
        m_data.pop_back();
    }

    bool contains(IdType id) const { return m_idToIndex.count(id); }

    using DerefType = std::conditional_t<isPointer, std::remove_pointer_t<T>, T>;
    DerefType* get(IdType id)
    {
        if (!contains(id)) return nullptr;
        T& stored = m_data[m_idToIndex.at(id)].second;
        if constexpr (isPointer) return stored;
        else return &stored;
    }
    const DerefType* get(IdType id) const
    {
        if (!contains(id)) return nullptr;
        const T& stored = m_data[m_idToIndex.at(id)].second;
        if constexpr (isPointer) return stored;
        else return &stored;
    }
};

}   // namespace Engine
