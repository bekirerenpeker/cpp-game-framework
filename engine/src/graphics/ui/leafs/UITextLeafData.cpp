#include "graphics/ui/leafs/UITextLeafData.hpp"
#include "graphics/text/TextLayoutCalculator.hpp"
#include "graphics/text/TextRenderer.hpp"

namespace Engine {

// Styles are resolved to concrete TextStyles here, on the way into the block: this is
// the last point before the text path, and TextBlock is what everything downstream --
// measuring, wrapping, drawing -- reads, so nothing past it can meet an unset field.
// The scale is passed in rather than read from the theme, so the leaf layer stays
// theme-free and UIManager remains the single place the UI system consults it.
UITextLeafData::UITextLeafData(const UITextConfig& config, float scale)
    : IUILeafData(UILeafType::Text), m_block(config.font, config.text, config.style.resolve(scale))
{
    std::vector<TextStyle> spanStyles;
    spanStyles.reserve(config.spanStyles.size());
    for (const UITextStyle& style : config.spanStyles) spanStyles.push_back(style.resolve(scale));

    m_block.setSpanStyles(spanStyles);
    m_block.setAlignH(config.alignment.horizontal);
    m_block.setAlignV(config.alignment.vertical);
    m_block.setOverflow(config.overflow);
    m_block.setWrapEnabled(config.wrapEnabled);
    m_block.setFixedLineHeight(config.fixedLineHeight);
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

void UITextLeafData::draw(Vec2 drawPos, Vec2 size, Vec4 clipRect)
{
    Vec2 topLeft = Vec2(drawPos.x - size.x * 0.5f, drawPos.y + size.y * 0.5f);

    // Cleared afterwards so world-space text drawn later in the frame is not still
    // bounded by whatever UI container happened to be emitted last.
    TextRenderer::get().setClipRect(clipRect);
    TextRenderer::get().draw(m_block, topLeft, size);
    TextRenderer::get().clearClipRect();
}

}   // namespace Engine
