#include "graphics/ui/UiLeaves.hpp"
#include "graphics/ui/TextMeasure.hpp"

namespace Engine {

void TextLeaf::set(
    const Font* font, std::string_view text, const TextStyle& style, bool wrapEnabled
)
{
    m_font = font;
    m_text.assign(text);
    m_style = style;
    m_wrapEnabled = wrapEnabled;
}

LeafWidths TextLeaf::measureWidths()
{
    if (!m_font || !m_font->isValid()) return LeafWidths {};

    float longestLine = TextMeasure::measureLongestLine(*m_font, m_text, m_style);
    if (!m_wrapEnabled) return LeafWidths {longestLine, longestLine};

    return LeafWidths {TextMeasure::measureLongestWord(*m_font, m_text, m_style), longestLine};
}

float TextLeaf::measureHeight(float contentWidth, std::vector<LayoutLine>& lines)
{
    if (!m_font || !m_font->isValid()) return 0.0f;

    // An unbounded budget makes wrap() break only on explicit newlines, which is
    // exactly the non-wrapping behaviour -- no second code path needed.
    float budget = m_wrapEnabled ? contentWidth : 0.0f;
    uint lineCount = TextMeasure::wrap(*m_font, m_text, m_style, budget, lines);
    return TextMeasure::heightForLines(*m_font, m_style, lineCount);
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
