#include "ui/UILayoutCalculator.hpp"
#include "ui/UIStateStore.hpp"
#include "ui/UiManager.hpp"
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

// Every read below dereferences: UIManager::addNode fills a node's layout and style on
// creation, so nothing that reaches the solver carries an unset field.
bool isFloating(const UILayoutNode& node) { return *node.node->layout.isFloating; }

// An overflow already says the box may be smaller than what is in it, so it carries the
// zero content floor with it rather than making the caller repeat itself in clipX/clipY.
// Those stay for a leaf, which has no style to put an overflow on but still has to be
// shrinkable -- a label that clips instead of pushing its row wider.
bool isShrinkable(const UILayoutNode& node, UILayoutAxis axis)
{
    if (*node.node->style.overflow != UIOverflow::Visible) return true;
    return axisClipped(node.node->layout, axis);
}

// Not the style field: a transparent border paints no ring however wide it is set, and
// the shader clamps to half the short side. Anything else here would carve pixels out
// of a ring that was never drawn, so this has to mirror UIRenderer's own rule exactly.
float paintedBorderWidth(const UILayoutNode& node)
{
    const UIContainerStyle& style = node.node->style;
    if (style.borderColor->a <= 0.0f) return 0.0f;

    float halfMin = Math::min(node.size.x, node.size.y) * 0.5f;
    return Math::clamp(*style.borderWidth, 0.0f, halfMin);
}

}   // namespace

const std::vector<UILayoutNode>&
UILayoutCalculator::calculate(IdType rootId, Vec2 rootTopLeft, Vec2 rootAvailable)
{
    m_nodes.clear();
    m_scratch.clear();
    m_rootAvailable = rootAvailable;

    if (buildSubtree(rootId, NO_LAYOUT_NODE) == NO_LAYOUT_NODE) return m_nodes;

    // Width and height are two separate cycles rather than one, because a text
    // leaf's height is a function of its final width while width never reads
    // height -- that is what keeps the solve non-iterative.
    computeIntrinsicWidths();
    computeFinalWidths();
    computeIntrinsicHeights();
    computeFinalHeights();
    // Between the sizes and the positions is the only point where both the viewport and
    // the content it holds are final, which is what lets the offset be clamped against
    // this frame's content rather than last frame's.
    resolveScroll();
    computePositions();
    applyTransforms();
    computeDrawPositions(rootTopLeft);
    computeClipRects();

    // Snapshotted for next frame's lookups, not this frame's draw -- .node would
    // dangle by then, so it is dropped here rather than left to rot.
    size_t base = m_prevFrameNodes.size();
    m_prevFrameNodes.insert(m_prevFrameNodes.end(), m_nodes.begin(), m_nodes.end());
    for (size_t i = base; i < m_prevFrameNodes.size(); i++) {
        UILayoutNode& node = m_prevFrameNodes[i];
        node.node = nullptr;
        node.rootOrder = m_rootOrder;
        // Index links are relative to this root's own solve, so they have to be
        // rebased onto the concatenated snapshot -- an un-rebased parent walk lands
        // in whichever root happens to sit at that offset.
        if (node.parent != NO_LAYOUT_NODE) node.parent += (uint)base;
        if (node.firstChild != NO_LAYOUT_NODE) node.firstChild += (uint)base;
        if (node.lastChild != NO_LAYOUT_NODE) node.lastChild += (uint)base;
        if (node.nextSibling != NO_LAYOUT_NODE) node.nextSibling += (uint)base;
    }
    m_rootOrder++;

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
    bool parentIgnoresInput =
        parentIndex == NO_LAYOUT_NODE ? false : m_nodes[parentIndex].ignoresInput;
    int zIndex = *node->style.zIndex;

    UILayoutNode layoutNode;
    layoutNode.node = node;
    layoutNode.persistentKey = node->persistentKey;
    layoutNode.ignoresInput = parentIgnoresInput || *node->style.ignoreInput;
    layoutNode.acceptsInput = node->isContainer() && node->isVisible && !layoutNode.ignoresInput;
    layoutNode.blocksInput = *node->style.blockInput;
    layoutNode.isScrollable = *node->style.overflow == UIOverflow::Scroll;
    layoutNode.cursor = *node->style.cursor;
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
        const UIEdges& padding = *layout.padding;

        if (m_nodes[index].node->leafData && m_nodes[index].firstChild == NO_LAYOUT_NODE) {
            UILeafWidths widths = m_nodes[index].node->measureWidths();
            m_nodes[index].minWidth = widths.min + padding.horizontal();
            m_nodes[index].maxWidth = widths.max + padding.horizontal();
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
        const UIEdges& padding = *layout.padding;

        if (m_nodes[index].node->leafData && m_nodes[index].firstChild == NO_LAYOUT_NODE) {
            float contentWidth = Math::max(m_nodes[index].size.x - padding.horizontal(), 0.0f);
            m_nodes[index].size.y =
                m_nodes[index].node->measureHeight(contentWidth) + padding.vertical();
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
    // Shrinking is allowed here even though nothing wraps vertically, and it changes
    // nothing for most nodes: a child that is not shrinkable already sits on its own
    // content as a floor, so it is filtered straight out and overflows exactly as
    // before. What it does buy is the viewport -- a Grow child seeds at max-content, and
    // without a shrink pass an overflowing scroll container would simply grow past its
    // parent instead of becoming a window onto its contents.
    for (size_t i = 0; i < m_nodes.size(); i++)
        distributeChildren((uint)i, UILayoutAxis::Vertical, true);
}

// What the children actually occupy, measured from their *solved* sizes rather than
// aggregated from intrinsics. The difference is the whole reason this is a pass of its
// own: an intrinsic walk asks a child how wide it would like to be, and a scrollable
// child answers with its content -- content it then clips and never paints. Reading the
// solved size instead makes it answer with its viewport, so a scroll container inside a
// scroll container does not hand its parent a range that reveals nothing.
Vec2 UILayoutCalculator::contentExtent(uint index) const
{
    const UILayoutConfig& layout = m_nodes[index].node->layout;
    UILayoutAxis mainAxis = mainAxisOf(*layout.direction);
    UILayoutAxis crossAxis = crossAxisOf(*layout.direction);

    float main = gapTotal(index);
    float cross = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;

        const UIEdges& margin = *m_nodes[child].node->layout.margin;
        main += sizeOf(m_nodes[child], mainAxis) + axisPadding(margin, mainAxis);
        cross =
            Math::max(cross, sizeOf(m_nodes[child], crossAxis) + axisPadding(margin, crossAxis));
    }

    Vec2 extent = VEC2_ZERO;
    axisSet(extent, mainAxis, main + axisPadding(*layout.padding, mainAxis));
    axisSet(extent, crossAxis, cross + axisPadding(*layout.padding, crossAxis));
    return extent;
}

// The offset is retained state, so it comes from the store rather than from the node --
// a caller cannot set it, which is the point: overflow = Scroll is the whole interface.
// Clamped and written straight back, so wheel input that ran past the end, or content
// that shrank since, is corrected here instead of one frame later.
void UILayoutCalculator::resolveScroll()
{
    UIStateStore& store = UIStateStore::get();

    for (uint i = 0; i < (uint)m_nodes.size(); i++) {
        UILayoutNode& node = m_nodes[i];
        if (!node.isScrollable) continue;

        node.contentSize = contentExtent(i);

        Vec2& scroll = store.systemState(node.persistentKey).scroll;
        scroll.x = Math::clamp(scroll.x, 0.0f, Math::max(node.contentSize.x - node.size.x, 0.0f));
        scroll.y = Math::clamp(scroll.y, 0.0f, Math::max(node.contentSize.y - node.size.y, 0.0f));
        node.scroll = scroll;
    }
}

void UILayoutCalculator::computePositions()
{
    if (m_nodes.empty()) return;

    m_nodes[0].pos = VEC2_ZERO;
    for (size_t i = 0; i < m_nodes.size(); i++) positionChildren((uint)i);
}

// Applied after siblings are positioned, so a hovered node growing/shifting rewrites
// only its own box, never reflowing anything else. Children are not re-anchored, so
// they stay where the untransformed box put them.
void UILayoutCalculator::applyTransforms()
{
    for (UILayoutNode& node : m_nodes) {
        const UILayoutConfig& layout = node.node->layout;
        const Vec2& offset = *layout.offset;
        const Vec2& scale = *layout.scale;
        if (offset == VEC2_ZERO && scale == VEC2_ONE) continue;

        Vec2 center = node.pos + node.size * 0.5f;
        node.size = node.size * scale;
        node.pos = center - node.size * 0.5f + offset;
    }
}

// The one place layout space (top-left, Y-down) converts to what the renderer wants
// (a centre, Y-up from rootTopLeft). Run last and stored rather than computed at draw
// time, since drawPos is also what hit testing compares against.
void UILayoutCalculator::computeDrawPositions(Vec2 rootTopLeft)
{
    for (UILayoutNode& node : m_nodes) {
        node.drawPos = Vec2(
            rootTopLeft.x + node.pos.x + node.size.x * 0.5f,
            rootTopLeft.y - (node.pos.y + node.size.y * 0.5f)
        );
    }
}

// Strictly top-down, which is free since the tree is already preorder: a parent's rect
// is final before its children are reached, so each node just narrows the box it
// inherited -- the intersection of every clipping ancestor, computed once per node
// instead of walked per draw.
void UILayoutCalculator::computeClipRects()
{
    for (UILayoutNode& node : m_nodes) {
        Vec4 inherited =
            node.parent == NO_LAYOUT_NODE ? UI_NO_CLIP : m_nodes[node.parent].childClipRect;
        // Dropped for the whole subtree, not just this node, which is what lets a popup
        // paint past the container it was declared in. Hit testing reads the same rect,
        // so an escaped node is clickable out there too.
        if (*node.node->style.ignoreClip) inherited = UI_NO_CLIP;

        node.clipRect = inherited;
        node.childClipRect = inherited;
        node.borderInset = paintedBorderWidth(node);

        UIOverflow overflow = *node.node->style.overflow;
        if (overflow == UIOverflow::Visible) continue;

        // drawPos is the centre in a y-up space, so the rect is centre +/- half. Inset by
        // the border, or children paint over the ring their parent drew inside this same
        // rect. Only childClipRect: the node's own quad still has to reach its own ring.
        Vec2 half = node.size * 0.5f - Vec2(node.borderInset);
        node.childClipRect = Vec4(
            Math::max(inherited.x, node.drawPos.x - half.x),
            Math::max(inherited.y, node.drawPos.y - half.y),
            Math::min(inherited.z, node.drawPos.x + half.x),
            Math::min(inherited.w, node.drawPos.y + half.y)
        );
    }
}

void UILayoutCalculator::aggregateIntrinsic(uint index, UILayoutAxis axis)
{
    const UILayoutConfig& layout = m_nodes[index].node->layout;
    bool alongMain = axis == mainAxisOf(*layout.direction);

    float min = 0.0f;
    float max = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;

        float childMargin = axisPadding(*m_nodes[child].node->layout.margin, axis);
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

    float pad = axisPadding(*layout.padding, axis);
    setIntrinsic(m_nodes[index], axis, min + pad, max + pad);
}

void UILayoutCalculator::finalizeIntrinsic(uint index, UILayoutAxis axis)
{
    const UILayoutConfig& layout = m_nodes[index].node->layout;
    const UISizeSpec& spec = axisSpec(layout, axis);

    float min = intrinsicMin(m_nodes[index], axis);
    float max = intrinsicMax(m_nodes[index], axis);

    // A shrinkable axis stops reporting its content as a floor, which is what lets a
    // scroll container be shrunk below its content instead of pushing siblings out.
    // Applied before Fixed rather than after: a fixed size is an instruction, not
    // content, so nothing about overflow should let a sibling squash it.
    if (isShrinkable(m_nodes[index], axis)) min = 0.0f;

    if (spec.mode == UISizeMode::Fixed) {
        min = spec.value;
        max = spec.value;
    }

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
    if (axis != mainAxisOf(*layout.direction)) {
        resolveCrossAxis(index, axis);
        return;
    }

    float inner = sizeOf(m_nodes[index], axis) - axisPadding(*layout.padding, axis);
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
    float inner = sizeOf(m_nodes[index], axis) - axisPadding(*layout.padding, axis);

    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        // Floating stays exempt from margin here too, matching the main-axis pass:
        // it is anchor-positioned, not flow-positioned, so there is no space "around
        // it" for a margin to carve out.
        float childInner = isFloating(m_nodes[child]) ?
                               inner :
                               inner - axisPadding(*m_nodes[child].node->layout.margin, axis);
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
        // A floating child never sat in the parent's flow, so the parent's inner size is
        // not a bound on it -- clamping to it collapses a Fit box anchored inside a
        // zero-width parent down to min-content, which for text is the longest word.
        size = isFloating(m_nodes[index]) ?
                   intrinsicMax(m_nodes[index], axis) :
                   Math::min(intrinsicMax(m_nodes[index], axis), Math::max(inner, floorValue));
        break;
    }
    return clampToSpec(index, axis, size);
}

void UILayoutCalculator::positionChildren(uint index)
{
    if (m_nodes[index].firstChild == NO_LAYOUT_NODE) return;

    const UILayoutConfig& layout = m_nodes[index].node->layout;
    UILayoutDirection direction = *layout.direction;
    UILayoutAxis mainAxis = mainAxisOf(direction);
    UILayoutAxis crossAxis = crossAxisOf(direction);

    const UIEdges& padding = *layout.padding;
    float innerMain = sizeOf(m_nodes[index], mainAxis) - axisPadding(padding, mainAxis);
    float innerCross = sizeOf(m_nodes[index], crossAxis) - axisPadding(padding, crossAxis);

    float used = gapTotal(index) + marginTotal(index, mainAxis);
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;
        used += sizeOf(m_nodes[child], mainAxis);
    }

    Vec2 origin = m_nodes[index].pos + padding.topLeft() - m_nodes[index].scroll;
    float cursor = axisGet(origin, mainAxis) + alignOffset(*layout.alignMain, innerMain - used);
    float crossOrigin = axisGet(origin, crossAxis);

    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) {
            positionFloatingChild(index, child);
            continue;
        }

        const UIEdges& childMargin = *m_nodes[child].node->layout.margin;
        cursor += axisLeading(childMargin, mainAxis);

        float childCross = sizeOf(m_nodes[child], crossAxis);
        float crossMargin = axisPadding(childMargin, crossAxis);
        Vec2 pos = VEC2_ZERO;
        axisSet(pos, mainAxis, cursor);
        axisSet(
            pos, crossAxis,
            crossOrigin + axisLeading(childMargin, crossAxis) +
                alignOffset(*layout.alignCross, innerCross - childCross - crossMargin)
        );
        m_nodes[child].pos = pos;

        cursor +=
            sizeOf(m_nodes[child], mainAxis) + axisTrailing(childMargin, mainAxis) + *layout.gap;
    }
}

void UILayoutCalculator::positionFloatingChild(uint parentIndex, uint childIndex)
{
    const UIFloatingConfig& floating = *m_nodes[childIndex].node->layout.floating;
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

// The available size is the window in screen space and nothing at all in world space,
// where there is no box for a ratio to be a ratio of -- so Grow and Percent fall back to
// max-content the way Fit does whenever it is absent, which is also what they did
// everywhere before a size was threaded in. Fit and Fixed never read it, which is what
// keeps every root that predates this identical. Deliberately not clamped to the content
// floor: a Fixed root narrower than its content is exactly what makes children shrink
// and text re-wrap.
float UILayoutCalculator::resolveRootSize(UILayoutAxis axis) const
{
    const UISizeSpec& spec = axisSpec(m_nodes[0].node->layout, axis);
    float available = axisGet(m_rootAvailable, axis);

    float size;
    switch (spec.mode) {
    case UISizeMode::Fixed: size = spec.value; break;
    case UISizeMode::Grow:
        size = available > 0.0f ? available : intrinsicMax(m_nodes[0], axis);
        break;
    case UISizeMode::Percent:
        size = available > 0.0f ? available * spec.value : intrinsicMax(m_nodes[0], axis);
        break;
    default: size = intrinsicMax(m_nodes[0], axis); break;
    }
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
    // The horizontal floor was already folded into minWidth by finalizeIntrinsic, but the
    // vertical axis stores a single height, so the same rule is re-applied here.
    if (axis == UILayoutAxis::Vertical && isShrinkable(m_nodes[index], axis) &&
        axisSpec(m_nodes[index].node->layout, axis).mode != UISizeMode::Fixed)
        return 0.0f;
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
    return *m_nodes[index].node->layout.gap * (float)(count - 1);
}

float UILayoutCalculator::marginTotal(uint index, UILayoutAxis axis) const
{
    float total = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_LAYOUT_NODE;
         child = m_nodes[child].nextSibling) {
        if (isFloating(m_nodes[child])) continue;
        total += axisPadding(*m_nodes[child].node->layout.margin, axis);
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
