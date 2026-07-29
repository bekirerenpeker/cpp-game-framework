#pragma once

#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>

namespace Engine {

constexpr uint NO_NODE = (uint)-1;
constexpr float UNBOUNDED = 1e30f;

enum class LayoutAxis : uint8_t
{
    Horizontal = 0,
    Vertical,
};

enum class LayoutDirection : uint8_t
{
    Row = 0,
    Column,
};

enum class SizeMode : uint8_t
{
    Fit = 0,
    Grow,
    Fixed,
    Percent,
};

enum class LayoutAlign : uint8_t
{
    Start = 0,
    Center,
    End,
};

struct SizeSpec
{
    SizeMode mode = SizeMode::Fit;
    float value = 0.0f;
    float min = 0.0f;
    float max = UNBOUNDED;

    static SizeSpec fit() { return SizeSpec {SizeMode::Fit, 0.0f, 0.0f, UNBOUNDED}; }
    static SizeSpec grow() { return SizeSpec {SizeMode::Grow, 0.0f, 0.0f, UNBOUNDED}; }
    static SizeSpec fixed(float value)
    {
        return SizeSpec {SizeMode::Fixed, value, 0.0f, UNBOUNDED};
    }
    static SizeSpec percent(float value)
    {
        return SizeSpec {SizeMode::Percent, value, 0.0f, UNBOUNDED};
    }
};

struct LayoutEdges
{
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    LayoutEdges() = default;
    LayoutEdges(float all) : left(all), right(all), top(all), bottom(all) {}
    LayoutEdges(float horizontal, float vertical)
        : left(horizontal), right(horizontal), top(vertical), bottom(vertical)
    {
    }
    LayoutEdges(float left, float right, float top, float bottom)
        : left(left), right(right), top(top), bottom(bottom)
    {
    }

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
    Vec2 topLeft() const { return Vec2(left, top); }
};

struct FloatingConfig
{
    Vec2 offset = VEC2_ZERO;
    LayoutAlign anchorX = LayoutAlign::Start;
    LayoutAlign anchorY = LayoutAlign::Start;
    LayoutAlign selfX = LayoutAlign::Start;
    LayoutAlign selfY = LayoutAlign::Start;
};

struct LayoutConfig
{
    SizeSpec width;
    SizeSpec height;
    LayoutEdges padding;
    FloatingConfig floating;
    Vec2 scrollOffset = VEC2_ZERO;
    float gap = 0.0f;
    LayoutDirection direction = LayoutDirection::Row;
    LayoutAlign alignMain = LayoutAlign::Start;
    LayoutAlign alignCross = LayoutAlign::Start;
    bool isFloating = false;
    bool clipX = false;
    bool clipY = false;
};

inline LayoutAxis mainAxisOf(LayoutDirection direction)
{
    return direction == LayoutDirection::Row ? LayoutAxis::Horizontal : LayoutAxis::Vertical;
}

inline LayoutAxis crossAxisOf(LayoutDirection direction)
{
    return direction == LayoutDirection::Row ? LayoutAxis::Vertical : LayoutAxis::Horizontal;
}

// Vec2 is a union of anonymous structs, so (&v.x)[axis] is not something to rely on.
inline float axisGet(const Vec2& v, LayoutAxis axis)
{
    return axis == LayoutAxis::Horizontal ? v.x : v.y;
}

inline void axisSet(Vec2& v, LayoutAxis axis, float value)
{
    if (axis == LayoutAxis::Horizontal) v.x = value;
    else v.y = value;
}

inline const SizeSpec& axisSpec(const LayoutConfig& config, LayoutAxis axis)
{
    return axis == LayoutAxis::Horizontal ? config.width : config.height;
}

inline float axisPadding(const LayoutEdges& padding, LayoutAxis axis)
{
    return axis == LayoutAxis::Horizontal ? padding.horizontal() : padding.vertical();
}

inline float axisPaddingStart(const LayoutEdges& padding, LayoutAxis axis)
{
    return axis == LayoutAxis::Horizontal ? padding.left : padding.top;
}

inline bool axisClipped(const LayoutConfig& config, LayoutAxis axis)
{
    return axis == LayoutAxis::Horizontal ? config.clipX : config.clipY;
}

}   // namespace Engine
