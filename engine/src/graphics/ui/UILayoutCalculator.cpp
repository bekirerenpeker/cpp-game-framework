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

const std::vector<UILayoutNode>& UILayoutCalculator::calculate(IdType rootId, Vec2 rootTopLeft)
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
    applyTransforms();
    computeDrawPositions(rootTopLeft);

    // Snapshotted for next frame's lookups, not this frame's draw -- .node would
    // dangle by then, so it is dropped here rather than left to rot.
    size_t base = m_prevFrameNodes.size();
    m_prevFrameNodes.insert(m_prevFrameNodes.end(), m_nodes.begin(), m_nodes.end());
    for (size_t i = base; i < m_prevFrameNodes.size(); i++) {
        UILayoutNode& node = m_prevFrameNodes[i];
        node.node = nullptr;
        // Index links are relative to this root's own solve, so they have to be
        // rebased onto the concatenated snapshot -- an un-rebased parent walk lands
        // in whichever root happens to sit at that offset.
        if (node.parent != NO_LAYOUT_NODE) node.parent += (uint)base;
        if (node.firstChild != NO_LAYOUT_NODE) node.firstChild += (uint)base;
        if (node.lastChild != NO_LAYOUT_NODE) node.lastChild += (uint)base;
        if (node.nextSibling != NO_LAYOUT_NODE) node.nextSibling += (uint)base;
    }

    return m_nodes;
}

const UILayoutNode* UILayoutCalculator::getPrevFrameLayout(uint64_t key) const
{
    for (const UILayoutNode& node : m_prevFrameNodes)
        if (node.persistentKey == key) return &node;
    return nullptr;
}

// Children are appended after their parent, so the array comes out in preorder and
// every bottom-up pass is a plain reverse loop with no recursion.
uint UILayoutCalculator::buildSubtree(IdType nodeId, uint parentIndex)
{
    UIManager& manager = UIManager::get();
    const UINode* node = manager.getNode(nodeId);
    if (!node) return NO_LAYOUT_NODE;

    uint index = (uint)m_nodes.size();
    // Read before the push_back: growing m_nodes can move the parent out from under a
    // reference to it. zIndex is relative to the parent rather than global, which is
    // what keeps a child from ever falling behind its own container.
    uint parentLayer = parentIndex == NO_LAYOUT_NODE ? 0 : m_nodes[parentIndex].paintLayer;
    int zIndex = node->style.zIndex.value_or(0);

    UILayoutNode layoutNode;
    layoutNode.node = node;
    layoutNode.persistentKey = node->persistentKey;
    layoutNode.acceptsInput = node->isContainer() && node->isVisible;
    layoutNode.paintLayer = parentLayer + (uint)(zIndex > 0 ? zIndex : 0);
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

// A cosmetic adjustment applied after siblings are already positioned, so a hovered
// node growing/shifting never reflows anything else -- it only rewrites its own box.
// That rewritten box is what ends up in the previous-frame snapshot too, so a scaled
// button's hit-test area tracks its drawn size rather than the pre-transform one.
// Does not re-anchor children: a transformed node with children would leave them
// sitting where the untransformed box put them.
void UILayoutCalculator::applyTransforms()
{
    for (UILayoutNode& node : m_nodes) {
        const UILayoutConfig& layout = node.node->layout;
        if (layout.offset == VEC2_ZERO && layout.scale == VEC2_ONE) continue;

        Vec2 center = node.pos + node.size * 0.5f;
        node.size = node.size * layout.scale;
        node.pos = center - node.size * 0.5f + layout.offset;
    }
}

// Display only, and deliberately the last thing to run: every geometric pass works
// in layout space (top-left anchored, Y-down) and this is the one place that is
// converted to what the renderer wants -- a centre, Y-up from rootTopLeft, which is
// where the caller wants the root's top-left corner to land. Placing the root here
// rather than at draw time is what keeps hit testing correct: drawPos is the one
// thing both the renderer and the mouse compare against, so it has to be final.
void UILayoutCalculator::computeDrawPositions(Vec2 rootTopLeft)
{
    for (UILayoutNode& node : m_nodes) {
        node.drawPos = Vec2(
            rootTopLeft.x + node.pos.x + node.size.x * 0.5f,
            rootTopLeft.y - (node.pos.y + node.size.y * 0.5f)
        );
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

        float childMargin = axisPadding(m_nodes[child].node->layout.margin, axis);
        float childMin = intrinsicMin(m_nodes[child], axis) + childMargin;
        float childMax = intrinsicMax(m_nodes[child], axis) + childMargin;
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
    float available = inner - gapTotal(index) - marginTotal(index, axis);

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
        // Floating stays exempt from margin here too, matching the main-axis pass:
        // it is anchor-positioned, not flow-positioned, so there is no space "around
        // it" for a margin to carve out.
        float childInner = isFloating(m_nodes[child]) ?
                               inner :
                               inner - axisPadding(m_nodes[child].node->layout.margin, axis);
        setSizeOf(m_nodes[child], axis, resolveChildAgainst(child, axis, childInner));
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

    float used = gapTotal(index) + marginTotal(index, mainAxis);
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

        const UIEdges& childMargin = m_nodes[child].node->layout.margin;
        cursor += axisLeading(childMargin, mainAxis);

        float childCross = sizeOf(m_nodes[child], crossAxis);
        float crossMargin = axisPadding(childMargin, crossAxis);
        Vec2 pos = VEC2_ZERO;
        axisSet(pos, mainAxis, cursor);
        axisSet(
            pos, crossAxis,
            crossOrigin + axisLeading(childMargin, crossAxis) +
                alignOffset(layout.alignCross, innerCross - childCross - crossMargin)
        );
        m_nodes[child].pos = pos;

        cursor +=
            sizeOf(m_nodes[child], mainAxis) + axisTrailing(childMargin, mainAxis) + layout.gap;
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

float UILayoutCalculator::marginTotal(uint index, UILayoutAxis axis) const
{
    float total = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;
        total += axisPadding(m_nodes[child].node->layout.margin, axis);
    }
    return total;
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
