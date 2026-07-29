#include "graphics/ui/elements/UiLeaves.hpp"

namespace Engine {

void TextLeaf::set(
    const Font* font, std::string_view text, const TextStyle& style,
    const std::vector<TextStyle>& spanStyles, bool wrapEnabled, bool fixedLineHeight
)
{
    m_font = font;
    m_style = style;
    m_spanStyles = spanStyles;
    m_wrapEnabled = wrapEnabled;
    m_fixedLineHeight = fixedLineHeight;

    // The spans are views into m_text, so the string has to be settled before the
    // parse runs or every view dangles on the next reallocation.
    m_text.assign(text);
    m_tagsClosed = TextTags::parse(m_text, m_spans);
}

const TextStyle& TextLeaf::styleFor(int styleIndex) const
{
    if (styleIndex < 0) return m_style;
    size_t index = (size_t)styleIndex;
    return index < m_spanStyles.size() ? m_spanStyles[index] : m_style;
}

LeafWidths TextLeaf::measureWidths()
{
    if (!m_font || !m_font->isValid()) return LeafWidths {};
    return TextMeasure::measureSpanWidths(*m_font, m_spans, m_style, m_spanStyles, m_wrapEnabled);
}

float TextLeaf::measureHeight(float contentWidth, std::vector<LayoutLine>& lines)
{
    if (!m_font || !m_font->isValid()) return 0.0f;

    // An unbounded budget makes wrapSpans break only on explicit newlines, which is
    // exactly the non-wrapping behaviour -- no second code path needed.
    float budget = m_wrapEnabled ? contentWidth : 0.0f;
    float height = TextMeasure::wrapSpans(
        *m_font, m_spans, m_style, m_spanStyles, budget, m_fixedLineHeight, m_runs, m_lines
    );

    lines.insert(lines.end(), m_lines.begin(), m_lines.end());
    return height;
}

void ImageLeaf::set(const GlTexture* texture, Vec2 nativeSize, float aspectRatio)
{
    m_texture = texture;
    m_nativeSize = nativeSize;
    m_aspectRatio = aspectRatio;
}

LeafWidths ImageLeaf::measureWidths()
{
    if (m_aspectRatio > 0.0f) return LeafWidths {0.0f, m_nativeSize.x};
    return LeafWidths {m_nativeSize.x, m_nativeSize.x};
}

float ImageLeaf::measureHeight(float contentWidth, std::vector<LayoutLine>& lines)
{
    (void)lines;
    if (m_aspectRatio > 0.0f) return contentWidth / m_aspectRatio;
    return m_nativeSize.y;
}

}   // namespace Engine
