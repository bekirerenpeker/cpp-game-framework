#pragma once

#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>

namespace Engine {

constexpr float UI_UNBOUNDED = 1e30f;

enum class UILayoutAxis : uint8_t
{
    Horizontal = 0,
    Vertical,
};

enum class UILayoutDirection : uint8_t
{
    Row = 0,
    Column,
};

enum class UISizeMode : uint8_t
{
    Fit = 0,
    Grow,
    Fixed,
    Percent,
};

enum class UIAlign : uint8_t
{
    Start = 0,
    Center,
    End,
};

struct UISizeSpec
{
    UISizeMode mode = UISizeMode::Fit;
    float value = 0.0f;
    float min = 0.0f;
    float max = UI_UNBOUNDED;

    static UISizeSpec fit() { return UISizeSpec {UISizeMode::Fit, 0.0f, 0.0f, UI_UNBOUNDED}; }
    static UISizeSpec grow() { return UISizeSpec {UISizeMode::Grow, 0.0f, 0.0f, UI_UNBOUNDED}; }
    static UISizeSpec fixed(float value)
    {
        return UISizeSpec {UISizeMode::Fixed, value, 0.0f, UI_UNBOUNDED};
    }
    static UISizeSpec percent(float value)
    {
        return UISizeSpec {UISizeMode::Percent, value, 0.0f, UI_UNBOUNDED};
    }
};

struct UIEdges
{
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;

    UIEdges() = default;
    UIEdges(float all) : left(all), right(all), top(all), bottom(all) {}
    UIEdges(float horizontal, float vertical)
        : left(horizontal), right(horizontal), top(vertical), bottom(vertical)
    {
    }
    UIEdges(float left, float right, float top, float bottom)
        : left(left), right(right), top(top), bottom(bottom)
    {
    }

    float horizontal() const { return left + right; }
    float vertical() const { return top + bottom; }
    Vec2 topLeft() const { return Vec2(left, top); }
};

// anchor picks the point on the parent, self picks the point on the child that
// lands on it -- Center/Center centres the child on the parent.
struct UIFloatingConfig
{
    Vec2 offset = VEC2_ZERO;
    UIAlign anchorX = UIAlign::Start;
    UIAlign anchorY = UIAlign::Start;
    UIAlign selfX = UIAlign::Start;
    UIAlign selfY = UIAlign::Start;
};

struct UILayoutConfig
{
    UISizeSpec width;
    UISizeSpec height;
    UIEdges padding;
    UIFloatingConfig floating;
    Vec2 scrollOffset = VEC2_ZERO;
    float gap = 0.0f;
    UILayoutDirection direction = UILayoutDirection::Row;
    UIAlign alignMain = UIAlign::Start;
    UIAlign alignCross = UIAlign::Start;
    bool isFloating = false;
    bool clipX = false;
    bool clipY = false;
};

inline UILayoutAxis mainAxisOf(UILayoutDirection direction)
{
    return direction == UILayoutDirection::Row ? UILayoutAxis::Horizontal : UILayoutAxis::Vertical;
}

inline UILayoutAxis crossAxisOf(UILayoutDirection direction)
{
    return direction == UILayoutDirection::Row ? UILayoutAxis::Vertical : UILayoutAxis::Horizontal;
}

inline const UISizeSpec& axisSpec(const UILayoutConfig& config, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? config.width : config.height;
}

inline float axisPadding(const UIEdges& padding, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? padding.horizontal() : padding.vertical();
}

inline bool axisClipped(const UILayoutConfig& config, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? config.clipX : config.clipY;
}

// Vec2 is a union of anonymous structs, so (&v.x)[axis] is not something to rely on.
inline float axisGet(const Vec2& v, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? v.x : v.y;
}

inline void axisSet(Vec2& v, UILayoutAxis axis, float value)
{
    if (axis == UILayoutAxis::Horizontal) v.x = value;
    else v.y = value;
}

}   // namespace Engine
