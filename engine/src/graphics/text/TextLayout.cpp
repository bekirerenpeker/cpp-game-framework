#include "graphics/text/TextLayout.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

namespace {

// A block caches its parse, so a static string warns once for the whole run rather
// than once per frame the way the old per-draw parser did.
bool g_warnedUnclosedTag = false;

constexpr float WIDTH_EPSILON = 0.0001f;

}   // namespace

TextBlock::TextBlock(const Font* font, std::string_view text, const TextStyle& style)
    : m_font(font), m_text(text), m_style(style)
{
}

void TextBlock::setFont(const Font* font)
{
    if (m_font == font) return;
    m_font = font;
    m_widthsDirty = true;
    m_layoutDirty = true;
}

// Runs and spans are views into m_text, so they are dropped before the string is
// reassigned rather than lazily on the next calculate -- there must be no window
// where a caller can read a run pointing into a freed buffer.
void TextBlock::setText(std::string_view text)
{
    if (m_text == text) return;
    clearLayout();
    m_spans.clear();
    m_text.assign(text);
    m_spansDirty = true;
    m_widthsDirty = true;
    m_layoutDirty = true;
}

void TextBlock::setStyle(const TextStyle& style)
{
    m_style = style;
    m_widthsDirty = true;
    m_layoutDirty = true;
}

void TextBlock::setSpanStyles(const std::vector<TextStyle>& spanStyles)
{
    m_spanStyles = spanStyles;
    m_widthsDirty = true;
    m_layoutDirty = true;
}

void TextBlock::setAlignH(TextAlignH alignH)
{
    if (m_alignH == alignH) return;
    m_alignH = alignH;
    m_layoutDirty = true;
}

// Vertical alignment needs the box height, which only the caller has, so it is
// applied at draw time and costs no relayout.
void TextBlock::setAlignV(TextAlignV alignV) { m_alignV = alignV; }

// Dirties the layout because Ellipsis cuts runs back during the solve; Clip alone would
// not need it, but the two share a setter and re-solving once is cheaper than the branch.
void TextBlock::setOverflow(TextOverflow overflow)
{
    if (m_overflow == overflow) return;
    m_overflow = overflow;
    m_layoutDirty = true;
}

void TextBlock::setWrapEnabled(bool wrapEnabled)
{
    if (m_wrapEnabled == wrapEnabled) return;
    m_wrapEnabled = wrapEnabled;
    m_widthsDirty = true;
    m_layoutDirty = true;
}

void TextBlock::setFixedLineHeight(bool fixedLineHeight)
{
    if (m_fixedLineHeight == fixedLineHeight) return;
    m_fixedLineHeight = fixedLineHeight;
    m_layoutDirty = true;
}

void TextBlock::invalidate()
{
    m_spansDirty = true;
    m_widthsDirty = true;
    m_layoutDirty = true;
}

void TextBlock::clearLayout()
{
    m_runs.clear();
    m_lines.clear();
    m_bounds = VEC2_ZERO;
    m_lastWidth = -1.0f;
}

bool TextBlock::isDirtyFor(float availableWidth) const
{
    if (m_layoutDirty) return true;
    // Compared even when wrapping is off: a non-wrapping block still needs the box
    // width to place a Center or Right alignment.
    return Math::abs(availableWidth - m_lastWidth) > WIDTH_EPSILON;
}

const TextStyle& TextBlock::styleForRun(int styleIndex) const
{
    // A style list shorter than the number of tagged spans is not an error -- the
    // extras just fall back to the default.
    if (styleIndex < 0 || (size_t)styleIndex >= m_spanStyles.size()) return m_style;
    return m_spanStyles[styleIndex];
}

void TextBlock::ensureSpans()
{
    if (!m_spansDirty) return;
    m_spansDirty = false;

    m_tagsClosed = TextTags::parse(m_text, m_spans);
    if (m_tagsClosed || g_warnedUnclosedTag) return;

    g_warnedUnclosedTag = true;
    LOG_WARNING(
        "text has an unclosed '{}{}' tag, so its last span runs to the end of the string: \"{}\" "
        "(further occurrences stay silent)",
        TextTags::TAG_PREFIX, TextTags::TAG_STYLE, m_text
    );
}

}   // namespace Engine
