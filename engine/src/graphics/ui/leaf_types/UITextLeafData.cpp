#include "graphics/ui/leaf_types/UITextLeafData.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/text/TextLayoutCalculator.hpp"
#include "graphics/text/TextRenderer.hpp"

namespace Engine {

UITextLeafData::UITextLeafData(
    const Font* font, std::string_view text, const TextStyle& textStyle,
    const TextAlignment& alignment, TextOverflow overflow
)
    : IUILeafData(UILeafType::Text), m_block(font, text, textStyle)
{
    m_block.setAlignH(alignment.horizontal);
    m_block.setAlignV(alignment.vertical);
    m_block.setOverflow(overflow);
}

UILeafWidths UITextLeafData::measureWidths()
{
    TextBlockWidths widths = TextLayoutCalculator::get().measureMinMaxWidth(m_block);
    return {widths.min, widths.max};
}

float UITextLeafData::measureHeight(float contentWidth)
{
    return TextLayoutCalculator::get().calculate(m_block, contentWidth);
}

void UITextLeafData::draw(Vec2 drawPos, Vec2 size)
{
    Renderer::get().addQuad(drawPos, size, Color(1.0f, 1.0f, 1.0f, 0.3f), nullptr);

    // TextRenderer walks its runs downward from a top-left origin, so the centre has
    // to be expanded back out to that corner -- upward, since drawPos is Y-up.
    Vec2 topLeft = Vec2(drawPos.x - size.x * 0.5f, drawPos.y + size.y * 0.5f);
    TextRenderer::get().draw(m_block, topLeft, size);
}

}   // namespace Engine
