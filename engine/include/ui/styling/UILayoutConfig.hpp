#pragma once

#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <cstdint>
#include <optional>
#include <vector>

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

// Stretch is the odd one out: it places nothing, it sizes. A Fit child under it fills the
// extent it is being aligned in instead of hugging its content, which is what makes a grid
// cell claim the whole band its span bought. It has no meaning along a flow's main axis,
// where Grow already says the same thing, so there it reads as Start.
enum class UIAlign : uint8_t
{
    Start = 0,
    Center,
    End,
    Stretch,
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

constexpr uint UI_GRID_DEFAULT_COLUMNS = 12;

// An empty column list means UI_GRID_DEFAULT_COLUMNS equal Grow tracks, which is the
// CSS-style default a cell carves up with gridSpan. Leaving it empty rather than
// materialising twelve specs is what keeps the default free of an allocation.
struct UIGridConfig
{
    std::vector<UISizeSpec> columns;
    float rowGap = -1.0f;   // < 0 means "same as gap"
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

// The field list lives once, here, and the struct, combine() and fillDefaults() are
// all generated from it -- adding a field is one line. Every field is optional for the
// same reason UIContainerStyle's are: combine() has to tell "caller left this
// untouched" apart from "caller set it to the same value the default happens to be,"
// and only absence can carry that, a concrete field never can. The third column is
// what UILayoutConfig's fields used to default-construct to before they became
// optional -- Fit/0/Row/Start, not a theme choice, just the plain fallback for a field
// nobody ever set. Unlike the style structs there is no per-state spec here -- no
// theme feeds this, and a layout that changed with hover would feed this frame's
// geometry off last frame's hit test and could oscillate, so state-reactive layout
// stays off the table.
#define UI_LAYOUT_CONFIG_FIELDS(X)                                                                 \
    X(UISizeSpec, width, UISizeSpec {})                                                            \
    X(UISizeSpec, height, UISizeSpec {})                                                           \
    X(UIEdges, padding, UIEdges {})                                                                \
    X(UIEdges, margin, UIEdges {})                                                                 \
    X(UIFloatingConfig, floating, UIFloatingConfig {})                                             \
    X(UIGridConfig, grid, UIGridConfig {})                                                         \
    X(Vec2, offset, VEC2_ZERO)                                                                     \
    X(Vec2, scale, VEC2_ONE)                                                                       \
    X(float, gap, 0.0f)                                                                            \
    X(uint, gridSpan, 1)                                                                           \
    X(UILayoutDirection, direction, UILayoutDirection::Row)                                        \
    X(UIAlign, alignMain, UIAlign::Start)                                                          \
    X(UIAlign, alignCross, UIAlign::Start)                                                         \
    X(bool, isGrid, false)                                                                         \
    X(bool, isFloating, false)                                                                     \
    X(bool, clipX, false)                                                                          \
    X(bool, clipY, false)

#define UI_LAYOUT_DECLARE_FIELD(type, name, def) std::optional<type> name;
#define UI_LAYOUT_MERGE_FIELD(type, name, def)                                                     \
    if (other.name) name = other.name;
#define UI_LAYOUT_FILL_DEFAULT_FIELD(type, name, def)                                              \
    if (!name) name = def;

struct UILayoutConfig
{
    UI_LAYOUT_CONFIG_FIELDS(UI_LAYOUT_DECLARE_FIELD)

    UILayoutConfig& combine(const UILayoutConfig& other)
    {
        UI_LAYOUT_CONFIG_FIELDS(UI_LAYOUT_MERGE_FIELD)
        return *this;
    }

    UILayoutConfig combined(const UILayoutConfig& other) const
    {
        UILayoutConfig result = *this;
        result.combine(other);
        return result;
    }

    UILayoutConfig& fillDefaults()
    {
        UI_LAYOUT_CONFIG_FIELDS(UI_LAYOUT_FILL_DEFAULT_FIELD)
        return *this;
    }
};

inline UILayoutAxis mainAxisOf(UILayoutDirection direction)
{
    return direction == UILayoutDirection::Row ? UILayoutAxis::Horizontal : UILayoutAxis::Vertical;
}

inline UILayoutAxis crossAxisOf(UILayoutDirection direction)
{
    return direction == UILayoutDirection::Row ? UILayoutAxis::Vertical : UILayoutAxis::Horizontal;
}

// Everything below dereferences rather than defaulting: UIManager::addNode runs
// fillDefaults on the layout of every node it creates, so a config that has reached the
// solver has no unset field left. Nothing here re-decides a default.
inline const UISizeSpec& axisSpec(const UILayoutConfig& config, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? *config.width : *config.height;
}

inline float axisPadding(const UIEdges& edges, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? edges.horizontal() : edges.vertical();
}

inline float axisLeading(const UIEdges& edges, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? edges.left : edges.top;
}

inline float axisTrailing(const UIEdges& edges, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? edges.right : edges.bottom;
}

// The explicit half of "this axis may be smaller than what is in it". overflow implies
// it and is what a container should set; these stay for a leaf, which has no style of
// its own to put an overflow on and still has to be shrinkable -- a clipped label.
inline bool axisClipped(const UILayoutConfig& config, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? *config.clipX : *config.clipY;
}

inline uint gridTrackCount(const UILayoutConfig& config)
{
    return config.grid->columns.empty() ? UI_GRID_DEFAULT_COLUMNS :
                                          (uint)config.grid->columns.size();
}

inline const UISizeSpec& gridTrackSpec(const UILayoutConfig& config, uint track)
{
    static const UISizeSpec DEFAULT_TRACK = UISizeSpec::grow();
    return config.grid->columns.empty() ? DEFAULT_TRACK : config.grid->columns[track];
}

inline float gridRowGap(const UILayoutConfig& config)
{
    return config.grid->rowGap < 0.0f ? *config.gap : config.grid->rowGap;
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
