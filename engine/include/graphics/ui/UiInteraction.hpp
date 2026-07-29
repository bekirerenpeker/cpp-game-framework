#pragma once

#include "graphics/ui/layout/LayoutTypes.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>
#include <string_view>

namespace Engine {

typedef uint64_t UiKey;
constexpr UiKey NO_UI_KEY = 0;

namespace UiHash {

constexpr UiKey FNV_OFFSET = 14695981039346656037ull;
constexpr UiKey FNV_PRIME = 1099511628211ull;

inline UiKey combine(UiKey seed, const void* data, size_t size)
{
    const byte* bytes = static_cast<const byte*>(data);
    UiKey hash = seed;
    for (size_t i = 0; i < size; i++) {
        hash ^= bytes[i];
        hash *= FNV_PRIME;
    }
    // NO_UI_KEY is a sentinel, so the one input that would collide with it is nudged
    // off rather than left to silently read as "no element".
    return hash == NO_UI_KEY ? FNV_PRIME : hash;
}

inline UiKey combine(UiKey seed, std::string_view text)
{
    return combine(seed, text.data(), text.size());
}

inline UiKey combine(UiKey seed, uint64_t value) { return combine(seed, &value, sizeof(value)); }

}   // namespace UiHash

struct UiState
{
    Vec2 pos = VEC2_ZERO;
    Vec2 size = VEC2_ZERO;
    Vec2 mouseLocal = VEC2_ZERO;
    Vec2 mouseNormalized = VEC2_ZERO;
    Vec2 mouseDelta = VEC2_ZERO;
    Vec2 dragDelta = VEC2_ZERO;

    uint index = NO_NODE;
    UiKey key = NO_UI_KEY;

    bool hasRect = false;
    bool isHovered = false;
    bool isHoveredDirect = false;
    bool isPressed = false;
    bool isHeld = false;
    bool isReleased = false;
    bool isClicked = false;
    bool isDragging = false;
};

}   // namespace Engine
