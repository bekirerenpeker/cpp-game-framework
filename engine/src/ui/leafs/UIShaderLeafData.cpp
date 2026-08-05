#include "ui/leafs/UIShaderLeafData.hpp"
#include "ui/UIRenderer.hpp"

namespace Engine {

UIShaderLeafData::UIShaderLeafData(const UIShaderConfig& config)
    : IUILeafData(UILeafType::Shader), m_config(config)
{
}

UILeafWidths UIShaderLeafData::measureWidths()
{
    return {m_config.intrinsicSize.x, m_config.intrinsicSize.x};
}

float UIShaderLeafData::measureHeight(float contentWidth) { return m_config.intrinsicSize.y; }

void UIShaderLeafData::draw(Vec2 drawPos, Vec2 size, Vec4 clipRect)
{
    UIRenderer::get().addShaderQuad(
        m_config.shader, drawPos, size, clipRect, m_config.params0, m_config.params1
    );
}

}   // namespace Engine
