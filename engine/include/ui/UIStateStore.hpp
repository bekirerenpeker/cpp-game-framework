#pragma once

#include "utils/Singleton.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>
#include <string_view>
#include <unordered_map>

namespace Engine {

constexpr uint64_t uiHashCombine(uint64_t seed, uint64_t value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

// FNV-1a, so a field name folds to a constant at the call site instead of being walked
// per lookup, and so a caller may hoist an id out of a frame loop if it ever matters.
constexpr uint64_t uiStateFieldId(std::string_view name)
{
    uint64_t hash = 14695981039346656037ULL;
    for (char c : name) {
        hash ^= (uint64_t)(unsigned char)c;
        hash *= 1099511628211ULL;
    }
    return hash;
}

// A bool living in a float slot, held by pointer into the store so a widget toggles its
// own state in place the way it did with the bool& it used to own. Copy assignment is
// deleted because it would rebind the slot rather than write through it.
struct UIStateFlag
{
    float* slot = nullptr;

    UIStateFlag& operator=(const UIStateFlag&) = delete;

    operator bool() const { return *slot > 0.5f; }
    UIStateFlag& operator=(bool value)
    {
        *slot = value ? 1.0f : 0.0f;
        return *this;
    }
};

// What the UI's own systems own, as opposed to the string-keyed values a widget stores:
// state the solver reads and writes every frame, which is why it is a named field it can
// reach rather than a hashed one.
struct UINodeSystemState
{
    Vec2 scroll = VEC2_ZERO;
};

class UIStateStore : public Singleton<UIStateStore>
{
    friend class Singleton<UIStateStore>;

  private:
    // Deliberately generous: a node goes untouched for as long as its tab is hidden, and
    // forgetting a hidden panel's scroll position is a worse bug than the few kilobytes a
    // stale entry costs. Sweeping at all is only necessary because the default node key is
    // positional, so a tree that changes shape orphans its old entries for good.
    static constexpr uint64_t SWEEP_INTERVAL_FRAMES = 3600;
    static constexpr uint64_t EVICT_AFTER_FRAMES = 36000;

    struct SystemEntry
    {
        UINodeSystemState state;
        uint64_t frame = 0;
    };

    struct ValueEntry
    {
        float value = 0.0f;
        uint64_t frame = 0;
    };

    std::unordered_map<uint64_t, SystemEntry> m_systemStates;
    std::unordered_map<uint64_t, ValueEntry> m_values;
    uint64_t m_frame = 0;
    uint64_t m_lastSweepFrame = 0;

  public:
    void beginFrame();

    UINodeSystemState& systemState(uint64_t nodeKey);
    const UINodeSystemState* trySystemState(uint64_t nodeKey) const;

    float& value(uint64_t nodeKey, std::string_view field, float initial = 0.0f);
    UIStateFlag flag(uint64_t nodeKey, std::string_view field, bool initial = false);

    Vec2 getVec2(uint64_t nodeKey, std::string_view field, Vec2 fallback = VEC2_ZERO);
    void setVec2(uint64_t nodeKey, std::string_view field, Vec2 value);

    // Rides the same float slot, which holds every integer up to 2^24 exactly -- far past
    // any index or count a widget keeps here. A pair rather than an int& because the slot
    // is a float and there is nothing to hand back a reference to.
    int getInt(uint64_t nodeKey, std::string_view field, int fallback = 0);
    void setInt(uint64_t nodeKey, std::string_view field, int value);

    bool contains(uint64_t nodeKey, std::string_view field) const;
    void clear();

  private:
    UIStateStore() = default;
    ~UIStateStore() = default;

    ValueEntry& entryOf(uint64_t nodeKey, std::string_view field, float initial);
    void sweep();
};

}   // namespace Engine
