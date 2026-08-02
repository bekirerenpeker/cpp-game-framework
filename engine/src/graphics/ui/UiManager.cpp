#include "graphics/ui/UiManager.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/text/TextRenderer.hpp"
#include "graphics/ui/UILayoutCalculator.hpp"
#include "graphics/ui/UIRenderer.hpp"
#include <functional>

namespace Engine {

namespace {

uint64_t hashCombine(uint64_t seed, uint64_t value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

uint64_t localKeyOf(std::string_view key, uint64_t positionalIndex)
{
    return key.empty() ? hashCombine(1, positionalIndex) :
                         hashCombine(2, std::hash<std::string_view> {}(key));
}

UINodeState computeState(IdType id, const UILayoutNode* prev)
{
    if (!prev) return {id, false, false, false, false};

    Vec2 half = prev->size * 0.5f;
    // Both sides come from the same space -- the mouse through UIRenderer, drawPos
    // baked by the solver -- so screen and world hit test through one path. drawPos is
    // centre-anchored and Y-up; flip into the box's own top-left/Y-down space so
    // local/relative match the rest of the UI instead of the render convention.
    Vec2 centerDelta = UIRenderer::get().getMouseUiPos() - prev->drawPos;
    Vec2 local = Vec2(centerDelta.x + half.x, half.y - centerDelta.y);
    Vec2 relative = prev->size.x > 0.0f && prev->size.y > 0.0f ? local / prev->size : VEC2_ZERO;

    bool hovered =
        local.x >= 0.0f && local.x <= prev->size.x && local.y >= 0.0f && local.y <= prev->size.y;

    return {
        id,
        hovered,
        hovered && Input::get().mouseButtonPressed(MouseButton::Left),
        hovered && Input::get().mouseButtonReleased(MouseButton::Left),
        hovered && Input::get().mouseButtonHeld(MouseButton::Left),
        relative,
        local
    };
}

}   // namespace

UIManager::~UIManager() { clear(); }

void UIManager::clear()
{
    for (IUILeafData* leaf : m_leaves) delete leaf;
    m_leaves.clear();
    m_nodes.clear();
    m_openStack.clear();
    m_roots.clear();
}

UINodeState UIManager::addNode(
    const UILayoutConfig& layout, const UIContainerStyle& style, std::string_view key
)
{
    IdType id = m_nodes.add();
    UINode* node = m_nodes.get(id);
    node->layout = layout;
    node->style = style;

    if (m_openStack.empty()) {
        node->parent = INVALID_ID;
        node->persistentKey = hashCombine(0, localKeyOf(key, (uint64_t)m_roots.size()));
        m_roots.push_back(id);
        return {id, false, false, false, false};
    }

    IdType parent = m_openStack.back();
    node->parent = parent;

    UINode* parentNode = m_nodes.get(parent);
    if (!parentNode) return {id, false, false, false, false};

    node->persistentKey =
        hashCombine(parentNode->persistentKey, localKeyOf(key, (uint64_t)parentNode->childCount));

    if (parentNode->lastChild == INVALID_ID) parentNode->firstChild = id;
    else m_nodes.get(parentNode->lastChild)->nextSibling = id;

    parentNode->lastChild = id;
    parentNode->childCount++;

    return {id, false, false, false, false};
}

UINodeState UIManager::openContainer(
    const UILayoutConfig& layout, const UIContainerStyle& style, std::string_view key
)
{
    UINodeState state = addNode(layout, style, key);
    UINode* node = m_nodes.get(state.id);
    if (node) {
        node->prevFrameLayout = UILayoutCalculator::get().getPrevFrameLayout(node->persistentKey);
        state = computeState(state.id, node->prevFrameLayout);
    }

    m_openStack.push_back(state.id);
    return state;
}

void UIManager::closeContainer()
{
    if (m_openStack.empty()) {
        LOG_ERROR(
            "closeContainer called with no container open; the open/close pairs are unbalanced"
        );
        return;
    }
    m_openStack.pop_back();
}

IdType UIManager::addTextLeaf(const UILayoutConfig& layout, const UITextConfig& config)
{
    UINodeState state = addNode(layout, {});
    UINode* node = m_nodes.get(state.id);
    if (!node) return state.id;

    UITextLeafData* leaf = new UITextLeafData(config);
    m_leaves.push_back(leaf);
    node->leafData = leaf;
    return state.id;
}

void UIManager::draw()
{
    if (!m_openStack.empty()) {
        LOG_ERROR(
            "{} container(s) left open at draw; every openContainer needs a closeContainer",
            m_openStack.size()
        );
    }

    UIRenderer& renderer = UIRenderer::get();

    // Glyphs have to reach the same space as the boxes. Setting the override flushes,
    // which is what keeps any world text submitted earlier this frame on the world
    // matrix instead of retroactively landing on this one.
    if (renderer.getSpace() == UISpace::World) TextRenderer::get().clearViewProjOverride();
    else TextRenderer::get().setViewProjOverride(renderer.getViewProjMat());

    UILayoutCalculator::get().beginFrame();

    Vec2 rootTopLeft = renderer.getRootOrigin();
    for (IdType rootId : m_roots) {
        const std::vector<UILayoutNode>& solved =
            UILayoutCalculator::get().calculate(rootId, rootTopLeft);
        for (const UILayoutNode& layoutNode : solved) {
            const UINode* node = layoutNode.node;
            if (!node) continue;
            node->draw(layoutNode.drawPos, layoutNode.size);
        }
    }

    // Boxes and glyphs are separate batches, so their relative depth is flush order,
    // not submission order. Resolving both here rather than leaving it to the caller
    // is the only way a box declared after a label reliably lands under it.
    renderer.flush();
    TextRenderer::get().flush();
    TextRenderer::get().clearViewProjOverride();
}

}   // namespace Engine
