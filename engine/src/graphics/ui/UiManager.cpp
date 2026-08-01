#include "graphics/ui/UiManager.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "graphics/ui/UILayoutCalculator.hpp"

namespace Engine {

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
IdType UIManager::addNode(const UILayoutConfig& layout, const UIContainerStyle& style)
{
    IdType id = m_nodes.add();
    UINode* node = m_nodes.get(id);
    node->layout = layout;
    node->style = style;

    if (m_openStack.empty()) {
        node->parent = INVALID_ID;
        m_roots.push_back(id);
        return id;
    }

    IdType parent = m_openStack.back();
    node->parent = parent;

    UINode* parentNode = m_nodes.get(parent);
    if (!parentNode) return id;

    if (parentNode->lastChild == INVALID_ID) parentNode->firstChild = id;
    else m_nodes.get(parentNode->lastChild)->nextSibling = id;

    parentNode->lastChild = id;
    parentNode->childCount++;

    return id;
}

IdType UIManager::openContainer(const UILayoutConfig& layout, const UIContainerStyle& style)
{
    IdType id = addNode(layout, style);
    m_openStack.push_back(id);
    return id;
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
    IdType id = addNode(layout, {});
    UINode* node = m_nodes.get(id);
    if (!node) return id;

    UITextLeafData* leaf = new UITextLeafData(config);
    m_leaves.push_back(leaf);
    node->leafData = leaf;
    return id;
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
    // in one frame cost nothing beyond their own subtree.
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
