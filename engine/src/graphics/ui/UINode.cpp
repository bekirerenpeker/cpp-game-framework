#include "graphics/ui/UINode.hpp"
#include "graphics/ui/UIRenderer.hpp"
#include "graphics/ui/leaf_types/IUILeafData.hpp"

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

void UINode::draw(Vec2 drawPos, Vec2 size) const
{
    if (!isVisible) return;

    if (leafData) {
        leafData->draw(drawPos, size);
        return;
    }

    UIRenderer::get().addContainerQuad(drawPos, size, style);
}

}   // namespace Engine
