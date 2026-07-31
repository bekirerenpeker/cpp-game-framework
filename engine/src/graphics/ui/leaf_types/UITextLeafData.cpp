#include "graphics/ui/leaf_types/UITextLeafData.hpp"
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

void UITextLeafData::draw(Vec2 pos, Vec2 size, const UINodeStyle& style)
{
    TextRenderer::get().draw(m_block, pos, size);
}

}   // namespace Engine
