#include "ui/UINode.hpp"
#include "ui/UIRenderer.hpp"
#include "ui/leafs/IUILeafData.hpp"

namespace Engine {

UILeafWidths UINode::measureWidths() const
{
    if (leafData) return leafData->measureWidths();
    return {};
}

float UINode::measureHeight(float contentWidth) const
{
    if (leafData) return leafData->measureHeight(contentWidth);
    return 0.0f;
}

void UINode::draw(Vec2 drawPos, Vec2 size, Vec4 clipRect) const
{
    if (!isVisible) return;

    if (leafData) {
        leafData->draw(drawPos, size, clipRect);
        return;
    }

    UIRenderer::get().addContainerQuad(drawPos, size, style, clipRect);
}

}   // namespace Engine
