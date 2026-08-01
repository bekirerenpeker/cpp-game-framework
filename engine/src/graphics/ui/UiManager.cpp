#include "graphics/ui/UiManager.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/ui/UILayoutCalculator.hpp"
#include <functional>

namespace Engine {

namespace {

uint64_t hashCombine(uint64_t seed, uint64_t value)
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

// Positional and explicit keys are combined with a different constant first so a
// container at sibling index 3 can never collide with an explicit key "3".
uint64_t localKeyOf(std::string_view key, uint64_t positionalIndex)
{
    return key.empty() ? hashCombine(1, positionalIndex) :
                         hashCombine(2, std::hash<std::string_view> {}(key));
}

}   // namespace

UIManager::~UIManager() { clear(); }

// The tree is meant to be declared fresh every frame, so this frees the leaves as
// well as the nodes -- a leaf is owned here precisely because a per-frame rebuild
// gives the caller nowhere sensible to keep it.
void UIManager::clear()
{
    for (IUILeafData* leaf : m_leaves) delete leaf;
    m_leaves.clear();
    m_nodes.clear();
    m_openStack.clear();
    m_roots.clear();
}

// Parenting comes from the open stack rather than an argument, so a caller never
// handles an id: whatever container is currently open adopts whatever is added next.
// persistentKey is a parent-scoped hash chain, not the per-frame id: it is what
// survives clear() so a container can be matched back up with its previous frame.
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
    if (node)
        node->prevFrameLayout = UILayoutCalculator::get().getPrevFrameLayout(node->persistentKey);

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

// A leaf is a node like any other -- it carries its own layout and the open
// container gives it a box. Only the content it draws differs, so it never opens.
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

    // Each top-level container is solved on its own, so several independent panels
    // in one frame cost nothing beyond their own subtree. beginFrame resets the
    // calculator's previous-frame snapshot once, so it accumulates every root's
    // result rather than only the last one solved.
    UILayoutCalculator::get().beginFrame();
    for (IdType rootId : m_roots) {
        const std::vector<UILayoutNode>& solved = UILayoutCalculator::get().calculate(rootId);
        for (const UILayoutNode& layoutNode : solved) {
            const UINode* node = layoutNode.node;
            if (!node) continue;
            node->draw(layoutNode.drawPos, layoutNode.size);
        }
    }
}

}   // namespace Engine
