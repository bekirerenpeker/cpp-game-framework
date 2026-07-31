#include "graphics/ui/UiManager.hpp"

namespace Engine {

IdType UIManager::addContainer(IdType parent, const UINodeStyle& style)
{
    IdType id = m_nodes.add();
    UINode* node = m_nodes.get(id);
    node->parent = parent;
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

}   // namespace Engine
