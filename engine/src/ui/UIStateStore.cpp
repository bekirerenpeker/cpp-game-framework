#include "ui/UIStateStore.hpp"

namespace Engine {

namespace {

// A Vec2's two halves are separate slots rather than the scalar slot plus an offset, so
// reading "pos" as a Vec2 and as a float lands on different entries -- mixing the two on
// one name is a caller error either way, and this makes it a silent miss instead of a
// half-overlap.
uint64_t axisSlot(uint64_t nodeKey, uint64_t fieldId, uint64_t axis)
{
    return uiHashCombine(nodeKey, uiHashCombine(fieldId, axis));
}

}   // namespace

// Eviction runs here, before a single widget has been declared, which is what makes
// handing out a float& safe: nothing can be holding one when the erase happens.
void UIStateStore::beginFrame()
{
    m_frame++;
    if (m_frame - m_lastSweepFrame < SWEEP_INTERVAL_FRAMES) return;

    m_lastSweepFrame = m_frame;
    sweep();
}

UINodeSystemState& UIStateStore::systemState(uint64_t nodeKey)
{
    SystemEntry& entry = m_systemStates[nodeKey];
    entry.frame = m_frame;
    return entry.state;
}

const UINodeSystemState* UIStateStore::trySystemState(uint64_t nodeKey) const
{
    auto found = m_systemStates.find(nodeKey);
    return found == m_systemStates.end() ? nullptr : &found->second.state;
}

float& UIStateStore::value(uint64_t nodeKey, std::string_view field, float initial)
{
    return entryOf(nodeKey, field, initial).value;
}

UIStateFlag UIStateStore::flag(uint64_t nodeKey, std::string_view field, bool initial)
{
    return {&entryOf(nodeKey, field, initial ? 1.0f : 0.0f).value};
}

// Reading counts as touching, or a value that is only ever written once and read
// thereafter would age out from under its owner.
Vec2 UIStateStore::getVec2(uint64_t nodeKey, std::string_view field, Vec2 fallback)
{
    uint64_t fieldId = uiStateFieldId(field);
    Vec2 result = fallback;

    auto x = m_values.find(axisSlot(nodeKey, fieldId, 0));
    if (x != m_values.end()) {
        x->second.frame = m_frame;
        result.x = x->second.value;
    }

    auto y = m_values.find(axisSlot(nodeKey, fieldId, 1));
    if (y != m_values.end()) {
        y->second.frame = m_frame;
        result.y = y->second.value;
    }

    return result;
}

void UIStateStore::setVec2(uint64_t nodeKey, std::string_view field, Vec2 value)
{
    uint64_t fieldId = uiStateFieldId(field);
    m_values[axisSlot(nodeKey, fieldId, 0)] = {value.x, m_frame};
    m_values[axisSlot(nodeKey, fieldId, 1)] = {value.y, m_frame};
}

int UIStateStore::getInt(uint64_t nodeKey, std::string_view field, int fallback)
{
    return (int)entryOf(nodeKey, field, (float)fallback).value;
}
void UIStateStore::setInt(uint64_t nodeKey, std::string_view field, int value)
{
    entryOf(nodeKey, field, (float)value).value = (float)value;
}

bool UIStateStore::contains(uint64_t nodeKey, std::string_view field) const
{
    return m_values.find(uiHashCombine(nodeKey, uiStateFieldId(field))) != m_values.end();
}

void UIStateStore::clear()
{
    m_systemStates.clear();
    m_values.clear();
}

UIStateStore::ValueEntry&
UIStateStore::entryOf(uint64_t nodeKey, std::string_view field, float initial)
{
    uint64_t slot = uiHashCombine(nodeKey, uiStateFieldId(field));
    auto result = m_values.try_emplace(slot, ValueEntry {initial, m_frame});
    result.first->second.frame = m_frame;
    return result.first->second;
}

void UIStateStore::sweep()
{
    for (auto it = m_values.begin(); it != m_values.end();) {
        if (m_frame - it->second.frame > EVICT_AFTER_FRAMES) it = m_values.erase(it);
        else ++it;
    }

    for (auto it = m_systemStates.begin(); it != m_systemStates.end();) {
        if (m_frame - it->second.frame > EVICT_AFTER_FRAMES) it = m_systemStates.erase(it);
        else ++it;
    }
}

}   // namespace Engine
