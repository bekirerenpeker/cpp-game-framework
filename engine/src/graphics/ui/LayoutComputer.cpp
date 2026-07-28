#include "graphics/ui/LayoutComputer.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

void LayoutComputer::reset(size_t expectedNodeCount)
{
    m_inputs.clear();
    m_nodes.clear();
    m_lines.clear();
    m_scratch.clear();
    if (expectedNodeCount > 0) {
        m_inputs.reserve(expectedNodeCount);
        m_nodes.reserve(expectedNodeCount);
    }
}

uint LayoutComputer::addNode(const LayoutInput& input)
{
    uint index = (uint)m_nodes.size();
    bool hasParent = input.parent != NO_NODE && input.parent < index;

#ifdef DEBUG
    // Preorder is an unchecked precondition of every pass: a bottom-up loop reaching
    // a node before its children have been computed produces silently wrong sizes.
    if (input.parent != NO_NODE && !hasParent && !m_warnedBadParent) {
        LOG_WARNING(
            "LayoutComputer: node {} declares parent {} which does not precede it; the tree "
            "must be added in preorder",
            index, input.parent
        );
        m_warnedBadParent = true;
    }
#endif

    LayoutNode node;
    node.parent = hasParent ? input.parent : NO_NODE;
    node.sourceIndex = input.sourceIndex;
    node.isFloating = input.config.isFloating;
    node.clipX = input.config.clipX;
    node.clipY = input.config.clipY;
    node.depth = hasParent ? m_nodes[input.parent].depth + 1 : 0;

    m_inputs.push_back(input);
    m_nodes.push_back(node);

    if (hasParent) {
        LayoutNode& parent = m_nodes[input.parent];
        if (parent.firstChild == NO_NODE) parent.firstChild = index;
        else m_nodes[parent.lastChild].nextSibling = index;
        parent.lastChild = index;
        parent.childCount++;
    }
    return index;
}

const std::vector<LayoutNode>& LayoutComputer::compute(Vec2 rootSize)
{
    m_rootSize = rootSize;
    m_lines.clear();

    computeIntrinsicWidths();
    computeFinalWidths();
    computeIntrinsicHeights();
    computeFinalHeights();
    computePositions();

    return m_nodes;
}

void LayoutComputer::computeIntrinsicWidths()
{
    for (size_t i = m_nodes.size(); i-- > 0;) {
        uint index = (uint)i;
        ILeafMeasurer* measurer = m_inputs[index].measurer;
        if (measurer != nullptr && m_nodes[index].firstChild == NO_NODE) {
            const LayoutEdges& padding = m_inputs[index].config.padding;
            LeafWidths widths = measurer->measureWidths();
            m_nodes[index].contentMin.x = widths.min + padding.horizontal();
            m_nodes[index].contentMax.x = widths.max + padding.horizontal();
        } else {
            aggregateIntrinsic(index, LayoutAxis::Horizontal);
        }
        finalizeIntrinsic(index, LayoutAxis::Horizontal);
    }
}

void LayoutComputer::computeFinalWidths()
{
    if (m_nodes.empty()) return;

    m_nodes[0].size.x = m_rootSize.x;
    for (size_t i = 0; i < m_nodes.size(); i++)
        distributeChildren((uint)i, LayoutAxis::Horizontal, true);
}

void LayoutComputer::computeIntrinsicHeights()
{
    for (size_t i = m_nodes.size(); i-- > 0;) {
        uint index = (uint)i;
        ILeafMeasurer* measurer = m_inputs[index].measurer;
        if (measurer != nullptr && m_nodes[index].firstChild == NO_NODE) {
            const LayoutEdges& padding = m_inputs[index].config.padding;
            float contentWidth = Math::max(m_nodes[index].size.x - padding.horizontal(), 0.0f);

            m_nodes[index].lineStart = (uint)m_lines.size();
            float height = measurer->measureHeight(contentWidth, m_lines);
            m_nodes[index].lineCount = (uint)m_lines.size() - m_nodes[index].lineStart;

            m_nodes[index].contentMin.y = height + padding.vertical();
            m_nodes[index].contentMax.y = m_nodes[index].contentMin.y;
        } else {
            aggregateIntrinsic(index, LayoutAxis::Vertical);
        }
        finalizeIntrinsic(index, LayoutAxis::Vertical);
    }
}

void LayoutComputer::computeFinalHeights()
{
    if (m_nodes.empty()) return;

    m_nodes[0].size.y = m_rootSize.y;
    // Nothing wraps vertically, so there is no height equivalent of the shrink pass:
    // content that does not fit overflows and is the clip flag's problem.
    for (size_t i = 0; i < m_nodes.size(); i++)
        distributeChildren((uint)i, LayoutAxis::Vertical, false);
}

void LayoutComputer::computePositions()
{
    if (m_nodes.empty()) return;

    m_nodes[0].pos = VEC2_ZERO;
    for (size_t i = 0; i < m_nodes.size(); i++) positionChildren((uint)i);
}

void LayoutComputer::aggregateIntrinsic(uint index, LayoutAxis axis)
{
    const LayoutConfig& config = m_inputs[index].config;
    bool alongMain = axis == mainAxisOf(config.direction);

    float min = 0.0f;
    float max = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_NODE;
         child = m_nodes[child].nextSibling) {
        if (m_nodes[child].isFloating) continue;

        float childMin = axisGet(m_nodes[child].contentMin, axis);
        float childMax = axisGet(m_nodes[child].contentMax, axis);
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

    float pad = axisPadding(config.padding, axis);
    axisSet(m_nodes[index].contentMin, axis, min + pad);
    axisSet(m_nodes[index].contentMax, axis, max + pad);
}

void LayoutComputer::finalizeIntrinsic(uint index, LayoutAxis axis)
{
    const LayoutConfig& config = m_inputs[index].config;
    const SizeSpec& spec = axisSpec(config, axis);

    float min = axisGet(m_nodes[index].contentMin, axis);
    float max = axisGet(m_nodes[index].contentMax, axis);

    if (spec.mode == SizeMode::Fixed) {
        min = spec.value;
        max = spec.value;
    }

    // A clipped axis stops reporting its content as a floor, which is what lets a
    // scroll container be shrunk below its content instead of pushing siblings out.
    if (axisClipped(config, axis)) min = 0.0f;

    min = Math::max(min, spec.min);
    min = Math::min(min, spec.max);
    max = Math::max(max, min);
    max = Math::min(max, spec.max);

    axisSet(m_nodes[index].contentMin, axis, min);
    axisSet(m_nodes[index].contentMax, axis, max);
}

void LayoutComputer::distributeChildren(uint index, LayoutAxis axis, bool allowShrink)
{
    if (m_nodes[index].firstChild == NO_NODE) return;

    const LayoutConfig& config = m_inputs[index].config;
    if (axis != mainAxisOf(config.direction)) {
        resolveCrossAxis(index, axis);
        return;
    }

    float inner = axisGet(m_nodes[index].size, axis) - axisPadding(config.padding, axis);
    float available = inner - gapTotal(index);

    m_scratch.clear();
    float used = 0.0f;
    for (uint child = m_nodes[index].firstChild; child != NO_NODE;
         child = m_nodes[child].nextSibling) {
        if (m_nodes[child].isFloating) {
            float size = resolveChildAgainst(child, axis, inner);
            axisSet(m_nodes[child].size, axis, size);
            continue;
        }
        float size = seedChildSize(child, axis, available);
        axisSet(m_nodes[child].size, axis, size);
        used += size;
        m_scratch.push_back(child);
    }

    float remaining = available - used;
    if (remaining > EPSILON) levelUp(axis, remaining);
    else if (remaining < -EPSILON && allowShrink) levelDown(axis, -remaining);
}

void LayoutComputer::resolveCrossAxis(uint index, LayoutAxis axis)
{
    const LayoutConfig& config = m_inputs[index].config;
    float inner = axisGet(m_nodes[index].size, axis) - axisPadding(config.padding, axis);

    for (uint child = m_nodes[index].firstChild; child != NO_NODE;
         child = m_nodes[child].nextSibling) {
        axisSet(m_nodes[child].size, axis, resolveChildAgainst(child, axis, inner));
    }
}

float LayoutComputer::resolveChildAgainst(uint index, LayoutAxis axis, float inner) const
{
    const SizeSpec& spec = axisSpec(m_inputs[index].config, axis);
    float floorValue = axisGet(m_nodes[index].contentMin, axis);

    float size = 0.0f;
    switch (spec.mode) {
    case SizeMode::Fixed  : size = spec.value; break;
    case SizeMode::Percent: size = spec.value * inner; break;
    case SizeMode::Grow   : size = Math::max(inner, floorValue); break;
    default:
        size = Math::min(axisGet(m_nodes[index].contentMax, axis), Math::max(inner, floorValue));
        break;
    }
    return clampToSpec(index, axis, size);
}

void LayoutComputer::positionChildren(uint index)
{
    if (m_nodes[index].firstChild == NO_NODE) return;

    const LayoutConfig& config = m_inputs[index].config;
    LayoutAxis mainAxis = mainAxisOf(config.direction);
    LayoutAxis crossAxis = crossAxisOf(config.direction);

    float innerMain =
        axisGet(m_nodes[index].size, mainAxis) - axisPadding(config.padding, mainAxis);
    float innerCross =
        axisGet(m_nodes[index].size, crossAxis) - axisPadding(config.padding, crossAxis);

    float used = gapTotal(index);
    for (uint child = m_nodes[index].firstChild; child != NO_NODE;
         child = m_nodes[child].nextSibling) {
        if (m_nodes[child].isFloating) continue;
        used += axisGet(m_nodes[child].size, mainAxis);
    }

    Vec2 origin = m_nodes[index].pos + config.padding.topLeft() - config.scrollOffset;
    float cursor = axisGet(origin, mainAxis) + alignOffset(config.alignMain, innerMain - used);
    float crossOrigin = axisGet(origin, crossAxis);

    for (uint child = m_nodes[index].firstChild; child != NO_NODE;
         child = m_nodes[child].nextSibling) {
        if (m_nodes[child].isFloating) {
            positionFloatingChild(index, child);
            continue;
        }

        float childCross = axisGet(m_nodes[child].size, crossAxis);
        Vec2 pos;
        axisSet(pos, mainAxis, cursor);
        axisSet(
            pos, crossAxis, crossOrigin + alignOffset(config.alignCross, innerCross - childCross)
        );
        m_nodes[child].pos = pos;

        cursor += axisGet(m_nodes[child].size, mainAxis) + config.gap;
    }
}

void LayoutComputer::positionFloatingChild(uint parentIndex, uint childIndex)
{
    const FloatingConfig& floating = m_inputs[childIndex].config.floating;
    const LayoutNode& parent = m_nodes[parentIndex];
    LayoutNode& child = m_nodes[childIndex];

    // The anchor picks a point on the parent, the self alignment picks the point on
    // the child that lands on it -- so Center/Center centres the child on the parent.
    float anchorX = parent.pos.x + alignOffset(floating.anchorX, parent.size.x);
    float anchorY = parent.pos.y + alignOffset(floating.anchorY, parent.size.y);
    float selfX = alignOffset(floating.selfX, child.size.x);
    float selfY = alignOffset(floating.selfY, child.size.y);

    child.pos = Vec2(anchorX - selfX + floating.offset.x, anchorY - selfY + floating.offset.y);
}

void LayoutComputer::levelUp(LayoutAxis axis, float remaining)
{
    size_t write = 0;
    for (size_t i = 0; i < m_scratch.size(); i++) {
        uint child = m_scratch[i];
        if (axisSpec(m_inputs[child].config, axis).mode != SizeMode::Grow) continue;
        if (axisGet(m_nodes[child].size, axis) >= upperBound(child, axis) - EPSILON) continue;
        m_scratch[write++] = child;
    }
    m_scratch.resize(write);

    while (remaining > EPSILON && !m_scratch.empty()) {
        float smallest = UNBOUNDED;
        float next = UNBOUNDED;
        for (uint child : m_scratch) {
            float size = axisGet(m_nodes[child].size, axis);
            if (size < smallest - EPSILON) {
                next = smallest;
                smallest = size;
            } else if (size > smallest + EPSILON && size < next) {
                next = size;
            }
        }

        size_t count = 0;
        for (uint child : m_scratch)
            if (axisGet(m_nodes[child].size, axis) <= smallest + EPSILON) count++;

        float step = remaining / (float)count;
        if (next < UNBOUNDED) step = Math::min(step, next - smallest);

        float applied = 0.0f;
        size_t keep = 0;
        for (size_t i = 0; i < m_scratch.size(); i++) {
            uint child = m_scratch[i];
            float size = axisGet(m_nodes[child].size, axis);
            float cap = upperBound(child, axis);
            float target = size <= smallest + EPSILON ? size + step : size;
            if (target > cap) target = cap;

            applied += target - size;
            axisSet(m_nodes[child].size, axis, target);
            if (target < cap - EPSILON) m_scratch[keep++] = child;
        }
        m_scratch.resize(keep);

        remaining -= applied;
        // A child capped by its own sizing.max stops absorbing; when every grower is
        // capped the surplus is left over and becomes alignMain's to place.
        if (applied <= EPSILON) break;
    }
}

void LayoutComputer::levelDown(LayoutAxis axis, float deficit)
{
    size_t write = 0;
    for (size_t i = 0; i < m_scratch.size(); i++) {
        uint child = m_scratch[i];
        if (axisGet(m_nodes[child].size, axis) > contentFloor(child, axis) + EPSILON)
            m_scratch[write++] = child;
    }
    m_scratch.resize(write);

    while (deficit > EPSILON && !m_scratch.empty()) {
        float largest = -UNBOUNDED;
        float next = -UNBOUNDED;
        for (uint child : m_scratch) {
            float size = axisGet(m_nodes[child].size, axis);
            if (size > largest + EPSILON) {
                next = largest;
                largest = size;
            } else if (size < largest - EPSILON && size > next) {
                next = size;
            }
        }

        size_t count = 0;
        for (uint child : m_scratch)
            if (axisGet(m_nodes[child].size, axis) >= largest - EPSILON) count++;

        float step = deficit / (float)count;
        if (next > -UNBOUNDED) step = Math::min(step, largest - next);

        float applied = 0.0f;
        size_t keep = 0;
        for (size_t i = 0; i < m_scratch.size(); i++) {
            uint child = m_scratch[i];
            float size = axisGet(m_nodes[child].size, axis);
            float floorValue = contentFloor(child, axis);
            float target = size >= largest - EPSILON ? size - step : size;
            if (target < floorValue) target = floorValue;

            applied += size - target;
            axisSet(m_nodes[child].size, axis, target);
            if (target > floorValue + EPSILON) m_scratch[keep++] = child;
        }
        m_scratch.resize(keep);

        deficit -= applied;
        // Every child sitting on its minimum and still not fitting is genuine
        // overflow, not a bug: the boxes spill and clipping is the caller's choice.
        if (applied <= EPSILON) break;
    }
}

float LayoutComputer::seedChildSize(uint index, LayoutAxis axis, float available) const
{
    const SizeSpec& spec = axisSpec(m_inputs[index].config, axis);

    float size = 0.0f;
    switch (spec.mode) {
    case SizeMode::Fixed  : size = spec.value; break;
    case SizeMode::Percent: size = spec.value * available; break;
    default:
        // Grow seeds at max-content, not min: seeding at min would let two growers
        // inside a Fit parent split the space evenly, wrapping the longer one while
        // the shorter one keeps slack.
        size = axisGet(m_nodes[index].contentMax, axis);
        break;
    }
    return clampToSpec(index, axis, size);
}

float LayoutComputer::clampToSpec(uint index, LayoutAxis axis, float value) const
{
    const SizeSpec& spec = axisSpec(m_inputs[index].config, axis);
    float result = Math::max(value, contentFloor(index, axis));
    result = Math::max(result, spec.min);
    return Math::min(result, spec.max);
}

float LayoutComputer::contentFloor(uint index, LayoutAxis axis) const
{
    return axisGet(m_nodes[index].contentMin, axis);
}

float LayoutComputer::upperBound(uint index, LayoutAxis axis) const
{
    return axisSpec(m_inputs[index].config, axis).max;
}

uint LayoutComputer::layoutChildCount(uint index) const
{
    uint count = 0;
    for (uint child = m_nodes[index].firstChild; child != NO_NODE;
         child = m_nodes[child].nextSibling) {
        if (!m_nodes[child].isFloating) count++;
    }
    return count;
}

float LayoutComputer::gapTotal(uint index) const
{
    uint count = layoutChildCount(index);
    if (count < 2) return 0.0f;
    return m_inputs[index].config.gap * (float)(count - 1);
}

float LayoutComputer::alignOffset(LayoutAlign align, float free)
{
    switch (align) {
    case LayoutAlign::Center: return free * 0.5f;
    case LayoutAlign::End   : return free;
    default                 : return 0.0f;
    }
}

}   // namespace Engine
