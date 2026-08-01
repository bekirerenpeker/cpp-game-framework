#include "graphics/ui/UiManager.hpp"
#include "graphics/ui/UILayoutCalculator.hpp"

namespace Engine {

IdType
UIManager::addContainer(IdType parent, const UILayoutConfig& layout, const UIContainerStyle& style)
{
    IdType id = m_nodes.add();
    UINode* node = m_nodes.get(id);
    node->parent = parent;
    node->layout = layout;
    node->style = style;

    if (parent == INVALID_ID) return id;

    UINode* parentNode = m_nodes.get(parent);
    if (!parentNode) return id;

    if (parentNode->lastChild == INVALID_ID) parentNode->firstChild = id;
    else m_nodes.get(parentNode->lastChild)->nextSibling = id;

    parentNode->lastChild = id;
    parentNode->childCount++;

    return id;
}

void UIManager::draw(IdType rootId, Vec2 rootSize)
{
    const auto& layout = UILayoutCalculator::get().calculate(rootId, rootSize);

    for (const auto& layoutNode : layout) {
        const UINode* node = layoutNode.node;
        if (!node) continue;
        node->draw(layoutNode.drawPos, layoutNode.size);
    }
}

}   // namespace Engine
