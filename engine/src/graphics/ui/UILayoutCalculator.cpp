#include "graphics/ui/UILayoutCalculator.hpp"
#include "graphics/ui/UiManager.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace {

float sizeOf(const UILayoutNode& node, UILayoutAxis axis) { return axisGet(node.size, axis); }

void setSizeOf(UILayoutNode& node, UILayoutAxis axis, float value)
{
    axisSet(node.size, axis, value);
}

// The vertical axis has no min/max pair: nothing wraps vertically, so a node's
// intrinsic height is one number living in size.y until the final pass overwrites it.
float intrinsicMin(const UILayoutNode& node, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? node.minWidth : node.size.y;
}

float intrinsicMax(const UILayoutNode& node, UILayoutAxis axis)
{
    return axis == UILayoutAxis::Horizontal ? node.maxWidth : node.size.y;
}

void setIntrinsic(UILayoutNode& node, UILayoutAxis axis, float min, float max)
{
    if (axis == UILayoutAxis::Horizontal) {
        node.minWidth = min;
        node.maxWidth = max;
    } else {
        node.size.y = max;
    }
}

bool isFloating(const UILayoutNode& node) { return node.node->layout.isFloating; }

}   // namespace

const std::vector<UILayoutNode>& UILayoutCalculator::calculate(IdType rootId)
{
    m_nodes.clear();
    m_scratch.clear();

    if (buildSubtree(rootId, NO_LAYOUT_NODE) == NO_LAYOUT_NODE) return m_nodes;

    // Width and height are two separate cycles rather than one, because a text
    // leaf's height is a function of its final width while width never reads
    // height -- that is what keeps the solve non-iterative.
    computeIntrinsicWidths();
    computeFinalWidths();
    computeIntrinsicHeights();
    computeFinalHeights();
    computePositions();
    computeDrawPositions();

    return m_nodes;
}

// Children are appended after their parent, so the array comes out in preorder and
// every bottom-up pass is a plain reverse loop with no recursion.
uint UILayoutCalculator::buildSubtree(IdType nodeId, uint parentIndex)
{
    UIManager& manager = UIManager::get();
    const UINode* node = manager.getNode(nodeId);
    if (!node) return NO_LAYOUT_NODE;

    uint index = (uint)m_nodes.size();
    UILayoutNode layoutNode;
    layoutNode.node = node;
    layoutNode.parent = parentIndex;
    m_nodes.push_back(layoutNode);

    if (parentIndex != NO_LAYOUT_NODE) {
        UILayoutNode& parent = m_nodes[parentIndex];
        if (parent.firstChild == NO_LAYOUT_NODE) parent.firstChild = index;
        else m_nodes[parent.lastChild].nextSibling = index;
        parent.lastChild = index;
        parent.childCount++;
    }

    for (IdType childId = node->firstChild; childId != INVALID_ID;) {
        const UINode* child = manager.getNode(childId);
        if (!child) break;
        buildSubtree(childId, index);
        childId = child->nextSibling;
    }

    return index;
}

void UILayoutCalculator::computeIntrinsicWidths()
{
    for (size_t i = m_nodes.size(); i-- > 0;) {
        uint index = (uint)i;
        const UILayoutConfig& layout = m_nodes[index].node->layout;

        if (m_nodes[index].node->leafData && m_nodes[index].firstChild == NO_LAYOUT_NODE) {
            UILeafWidths widths = m_nodes[index].node->measureWidths();
            m_nodes[index].minWidth = widths.min + layout.padding.horizontal();
            m_nodes[index].maxWidth = widths.max + layout.padding.horizontal();
        } else {
            aggregateIntrinsic(index, UILayoutAxis::Horizontal);
        }
        finalizeIntrinsic(index, UILayoutAxis::Horizontal);
    }
}

void UILayoutCalculator::computeFinalWidths()
{
    if (m_nodes.empty()) return;

    m_nodes[0].size.x = resolveRootSize(UILayoutAxis::Horizontal);
    for (size_t i = 0; i < m_nodes.size(); i++)
        distributeChildren((uint)i, UILayoutAxis::Horizontal, true);
}

void UILayoutCalculator::computeIntrinsicHeights()
{
    for (size_t i = m_nodes.size(); i-- > 0;) {
        uint index = (uint)i;
        const UILayoutConfig& layout = m_nodes[index].node->layout;

        if (m_nodes[index].node->leafData && m_nodes[index].firstChild == NO_LAYOUT_NODE) {
            float contentWidth =
                Math::max(m_nodes[index].size.x - layout.padding.horizontal(), 0.0f);
            m_nodes[index].size.y =
                m_nodes[index].node->measureHeight(contentWidth) + layout.padding.vertical();
        } else {
            aggregateIntrinsic(index, UILayoutAxis::Vertical);
        }
        finalizeIntrinsic(index, UILayoutAxis::Vertical);
    }
}

void UILayoutCalculator::computeFinalHeights()
{
    if (m_nodes.empty()) return;

    m_nodes[0].size.y = resolveRootSize(UILayoutAxis::Vertical);
    // Nothing wraps vertically, so there is no height equivalent of the shrink pass:
    // content that does not fit overflows and is the clip flag's problem.
    for (size_t i = 0; i < m_nodes.size(); i++)
        distributeChildren((uint)i, UILayoutAxis::Vertical, false);
}

void UILayoutCalculator::computePositions()
{
    if (m_nodes.empty()) return;

    m_nodes[0].pos = VEC2_ZERO;
    for (size_t i = 0; i < m_nodes.size(); i++) positionChildren((uint)i);
}

// Display only, and deliberately the last thing to run: every geometric pass works
// in layout space (top-left anchored, Y-down) and this is the one place that is
// converted to what the renderer wants -- a centre, Y-up from the root's bottom.
void UILayoutCalculator::computeDrawPositions()
{
    float rootHeight = m_nodes[0].size.y;
    for (UILayoutNode& node : m_nodes) {
        node.drawPos =
            Vec2(node.pos.x + node.size.x * 0.5f, rootHeight - (node.pos.y + node.size.y * 0.5f));
    }
}

void UILayoutCalculator::aggregateIntrinsic(uint index, UILayoutAxis axis)
{
    const UILayoutConfig& layout = m_nodes[index].node->layout;
    bool alongMain = axis == mainAxisOf(layout.direction);

    float min = 0.0f;
    float max = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;

        float childMin = intrinsicMin(m_nodes[child], axis);
        float childMax = intrinsicMax(m_nodes[child], axis);
        if (alongMain) {
            min += childMin;
            max += childMax;
        } else {
            min = Math::max(min, childMin);
            max = Math::max(max, childMax);
        }
    }

    if (alongMain) {
        float gaps = gapTotal(index);
        min += gaps;
        max += gaps;
    }

    float pad = axisPadding(layout.padding, axis);
    setIntrinsic(m_nodes[index], axis, min + pad, max + pad);
}

void UILayoutCalculator::finalizeIntrinsic(uint index, UILayoutAxis axis)
{
    const UILayoutConfig& layout = m_nodes[index].node->layout;
    const UISizeSpec& spec = axisSpec(layout, axis);

    float min = intrinsicMin(m_nodes[index], axis);
    float max = intrinsicMax(m_nodes[index], axis);

    if (spec.mode == UISizeMode::Fixed) {
        min = spec.value;
        max = spec.value;
    }

    // A clipped axis stops reporting its content as a floor, which is what lets a
    // scroll container be shrunk below its content instead of pushing siblings out.
    if (axisClipped(layout, axis)) min = 0.0f;

    min = Math::max(min, spec.min);
    min = Math::min(min, spec.max);
    max = Math::max(max, min);
    max = Math::min(max, spec.max);

    setIntrinsic(m_nodes[index], axis, min, max);
}

void UILayoutCalculator::distributeChildren(uint index, UILayoutAxis axis, bool allowShrink)
{
    if (m_nodes[index].firstChild == NO_LAYOUT_NODE) return;

    const UILayoutConfig& layout = m_nodes[index].node->layout;
    if (axis != mainAxisOf(layout.direction)) {
        resolveCrossAxis(index, axis);
        return;
    }

    float inner = sizeOf(m_nodes[index], axis) - axisPadding(layout.padding, axis);
    float available = inner - gapTotal(index);

    m_scratch.clear();
    float used = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) {
            setSizeOf(m_nodes[child], axis, resolveChildAgainst(child, axis, inner));
            continue;
        }
        float size = seedChildSize(child, axis, available);
        setSizeOf(m_nodes[child], axis, size);
        used += size;
        m_scratch.push_back(child);
    }

    float remaining = available - used;
    if (remaining > EPSILON) levelUp(axis, remaining);
    else if (remaining < -EPSILON && allowShrink) levelDown(axis, -remaining);
}

void UILayoutCalculator::resolveCrossAxis(uint index, UILayoutAxis axis)
{
    const UILayoutConfig& layout = m_nodes[index].node->layout;
    float inner = sizeOf(m_nodes[index], axis) - axisPadding(layout.padding, axis);

    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        setSizeOf(m_nodes[child], axis, resolveChildAgainst(child, axis, inner));
    }
}

float UILayoutCalculator::resolveChildAgainst(uint index, UILayoutAxis axis, float inner) const
{
    const UISizeSpec& spec = axisSpec(m_nodes[index].node->layout, axis);
    float floorValue = contentFloor(index, axis);

    float size = 0.0f;
    switch (spec.mode) {
    case UISizeMode::Fixed  : size = spec.value; break;
    case UISizeMode::Percent: size = spec.value * inner; break;
    case UISizeMode::Grow   : size = Math::max(inner, floorValue); break;
    default:
        size = Math::min(intrinsicMax(m_nodes[index], axis), Math::max(inner, floorValue));
        break;
    }
    return clampToSpec(index, axis, size);
}

void UILayoutCalculator::positionChildren(uint index)
{
    if (m_nodes[index].firstChild == NO_LAYOUT_NODE) return;

    const UILayoutConfig& layout = m_nodes[index].node->layout;
    UILayoutAxis mainAxis = mainAxisOf(layout.direction);
    UILayoutAxis crossAxis = crossAxisOf(layout.direction);

    float innerMain = sizeOf(m_nodes[index], mainAxis) - axisPadding(layout.padding, mainAxis);
    float innerCross = sizeOf(m_nodes[index], crossAxis) - axisPadding(layout.padding, crossAxis);

    float used = gapTotal(index);
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;
        used += sizeOf(m_nodes[child], mainAxis);
    }

    Vec2 origin = m_nodes[index].pos + layout.padding.topLeft() - layout.scrollOffset;
    float cursor = axisGet(origin, mainAxis) + alignOffset(layout.alignMain, innerMain - used);
    float crossOrigin = axisGet(origin, crossAxis);

    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) {
            positionFloatingChild(index, child);
            continue;
        }

        float childCross = sizeOf(m_nodes[child], crossAxis);
        Vec2 pos = VEC2_ZERO;
        axisSet(pos, mainAxis, cursor);
        axisSet(
            pos, crossAxis, crossOrigin + alignOffset(layout.alignCross, innerCross - childCross)
        );
        m_nodes[child].pos = pos;

        cursor += sizeOf(m_nodes[child], mainAxis) + layout.gap;
    }
}

void UILayoutCalculator::positionFloatingChild(uint parentIndex, uint childIndex)
{
    const UIFloatingConfig& floating = m_nodes[childIndex].node->layout.floating;
    const UILayoutNode& parent = m_nodes[parentIndex];
    UILayoutNode& child = m_nodes[childIndex];

    float anchorX = parent.pos.x + alignOffset(floating.anchorX, parent.size.x);
    float anchorY = parent.pos.y + alignOffset(floating.anchorY, parent.size.y);
    float selfX = alignOffset(floating.selfX, child.size.x);
    float selfY = alignOffset(floating.selfY, child.size.y);

    child.pos = Vec2(anchorX - selfX + floating.offset.x, anchorY - selfY + floating.offset.y);
}

void UILayoutCalculator::levelUp(UILayoutAxis axis, float remaining)
{
    size_t write = 0;
    for (size_t i = 0; i < m_scratch.size(); i++) {
        uint child = m_scratch[i];
        if (axisSpec(m_nodes[child].node->layout, axis).mode != UISizeMode::Grow) continue;
        if (sizeOf(m_nodes[child], axis) >= upperBound(child, axis) - EPSILON) continue;
        m_scratch[write++] = child;
    }
    m_scratch.resize(write);

    while (remaining > EPSILON && !m_scratch.empty()) {
        float smallest = UI_UNBOUNDED;
        float next = UI_UNBOUNDED;
        for (uint child : m_scratch) {
            float size = sizeOf(m_nodes[child], axis);
            if (size < smallest - EPSILON) {
                next = smallest;
                smallest = size;
            } else if (size > smallest + EPSILON && size < next) {
                next = size;
            }
        }

        size_t count = 0;
        for (uint child : m_scratch)
            if (sizeOf(m_nodes[child], axis) <= smallest + EPSILON) count++;

        float step = remaining / (float)count;
        if (next < UI_UNBOUNDED) step = Math::min(step, next - smallest);

        float applied = 0.0f;
        size_t keep = 0;
        for (size_t i = 0; i < m_scratch.size(); i++) {
            uint child = m_scratch[i];
            float size = sizeOf(m_nodes[child], axis);
            float cap = upperBound(child, axis);
            float target = size <= smallest + EPSILON ? size + step : size;
            if (target > cap) target = cap;

            applied += target - size;
            setSizeOf(m_nodes[child], axis, target);
            if (target < cap - EPSILON) m_scratch[keep++] = child;
        }
        m_scratch.resize(keep);

        remaining -= applied;
        // A child capped by its own sizing.max stops absorbing; when every grower is
        // capped the surplus is left over and becomes alignMain's to place.
        if (applied <= EPSILON) break;
    }
}

void UILayoutCalculator::levelDown(UILayoutAxis axis, float deficit)
{
    size_t write = 0;
    for (size_t i = 0; i < m_scratch.size(); i++) {
        uint child = m_scratch[i];
        if (sizeOf(m_nodes[child], axis) > contentFloor(child, axis) + EPSILON)
            m_scratch[write++] = child;
    }
    m_scratch.resize(write);

    while (deficit > EPSILON && !m_scratch.empty()) {
        float largest = -UI_UNBOUNDED;
        float next = -UI_UNBOUNDED;
        for (uint child : m_scratch) {
            float size = sizeOf(m_nodes[child], axis);
            if (size > largest + EPSILON) {
                next = largest;
                largest = size;
            } else if (size < largest - EPSILON && size > next) {
                next = size;
            }
        }

        size_t count = 0;
        for (uint child : m_scratch)
            if (sizeOf(m_nodes[child], axis) >= largest - EPSILON) count++;

        float step = deficit / (float)count;
        if (next > -UI_UNBOUNDED) step = Math::min(step, largest - next);

        float applied = 0.0f;
        size_t keep = 0;
        for (size_t i = 0; i < m_scratch.size(); i++) {
            uint child = m_scratch[i];
            float size = sizeOf(m_nodes[child], axis);
            float floorValue = contentFloor(child, axis);
            float target = size >= largest - EPSILON ? size - step : size;
            if (target < floorValue) target = floorValue;

            applied += size - target;
            setSizeOf(m_nodes[child], axis, target);
            if (target > floorValue + EPSILON) m_scratch[keep++] = child;
        }
        m_scratch.resize(keep);

        deficit -= applied;
        // Every child sitting on its minimum and still not fitting is genuine
        // overflow, not a bug: the boxes spill and clipping is the caller's choice.
        if (applied <= EPSILON) break;
    }
}

// A root answers to nothing, so its own spec is the whole story. Grow and Percent
// have no parent box to resolve against and fall back to max-content the way Fit
// does. Deliberately not clamped to the content floor: a Fixed root narrower than
// its content is exactly what makes the children shrink and the text re-wrap.
float UILayoutCalculator::resolveRootSize(UILayoutAxis axis) const
{
    const UISizeSpec& spec = axisSpec(m_nodes[0].node->layout, axis);
    float size = spec.mode == UISizeMode::Fixed ? spec.value : intrinsicMax(m_nodes[0], axis);
    return Math::min(Math::max(size, spec.min), spec.max);
}

float UILayoutCalculator::seedChildSize(uint index, UILayoutAxis axis, float available) const
{
    const UISizeSpec& spec = axisSpec(m_nodes[index].node->layout, axis);

    float size = 0.0f;
    switch (spec.mode) {
    case UISizeMode::Fixed  : size = spec.value; break;
    case UISizeMode::Percent: size = spec.value * available; break;
    default:
        // Grow seeds at max-content, not min: seeding at min would let two growers
        // inside a Fit parent split the space evenly, wrapping the longer one while
        // the shorter one keeps slack.
        size = intrinsicMax(m_nodes[index], axis);
        break;
    }
    return clampToSpec(index, axis, size);
}

float UILayoutCalculator::clampToSpec(uint index, UILayoutAxis axis, float value) const
{
    const UISizeSpec& spec = axisSpec(m_nodes[index].node->layout, axis);
    float result = Math::max(value, contentFloor(index, axis));
    result = Math::max(result, spec.min);
    return Math::min(result, spec.max);
}

float UILayoutCalculator::contentFloor(uint index, UILayoutAxis axis) const
{
    // clipX was already folded into minWidth by finalizeIntrinsic, but the vertical
    // axis stores a single height, so clipY has to be honoured here instead.
    if (axis == UILayoutAxis::Vertical && m_nodes[index].node->layout.clipY) return 0.0f;
    return intrinsicMin(m_nodes[index], axis);
}

float UILayoutCalculator::upperBound(uint index, UILayoutAxis axis) const
{
    return axisSpec(m_nodes[index].node->layout, axis).max;
}

uint UILayoutCalculator::layoutChildCount(uint index) const
{
    uint count = 0;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (!isFloating(m_nodes[child])) count++;
    }
    return count;
}

float UILayoutCalculator::gapTotal(uint index) const
{
    uint count = layoutChildCount(index);
    if (count < 2) return 0.0f;
    return m_nodes[index].node->layout.gap * (float)(count - 1);
}

float UILayoutCalculator::alignOffset(UIAlign align, float free)
{
    switch (align) {
    case UIAlign::Center: return free * 0.5f;
    case UIAlign::End   : return free;
    default             : return 0.0f;
    }
}

}   // namespace Engine
