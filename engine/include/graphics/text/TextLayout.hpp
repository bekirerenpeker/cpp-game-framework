#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/text/TextTags.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace Engine {

enum class TextAlignH
{
    Left = 0,
    Center,
    Right,
};

enum class TextAlignV
{
    Top = 0,
    Middle,
    Bottom,
};

struct TextAlignment
{
    TextAlignH horizontal = TextAlignH::Left;
    TextAlignV vertical = TextAlignV::Top;
};

// Declared, not honoured: every value currently lays out and draws as Visible.
enum class TextOverflow
{
    Visible = 0,
    Clip,
    Ellipsis,
};

// A slice of one span sitting on one line. offset is Y-down from the block's
// top-left: offset.x is the left edge with alignment already baked in, offset.y is
// the line's baseline.
struct TextRun
{
    std::string_view text;
    int styleIndex = -1;
    Vec2 offset = VEC2_ZERO;
};

// height is the step to the next line -- the tallest style on this line rather than
// any single style's; ascent places the baseline inside that step. descent is
// positive downwards, top is Y-down from the block's top-left.
struct TextLine
{
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    uint firstRun = 0;
    uint runCount = 0;
};

struct TextBlockWidths
{
    float min = 0.0f;
    float max = 0.0f;
};

// Input and computed layout for one piece of text. Copy and move are deleted
// because m_runs holds views into m_text, and a short string moves by copying into
// the destination's inline storage -- every view would silently dangle.
class TextBlock
{
    friend class TextLayoutCalculator;

  private:
    const Font* m_font = nullptr;
    std::string m_text;
    TextStyle m_style;
    std::vector<TextStyle> m_spanStyles;
    TextAlignH m_alignH = TextAlignH::Left;
    TextAlignV m_alignV = TextAlignV::Top;
    TextOverflow m_overflow = TextOverflow::Visible;
    bool m_wrapEnabled = true;
    bool m_fixedLineHeight = false;

    std::vector<TextSpan> m_spans;
    std::vector<TextRun> m_runs;
    std::vector<TextLine> m_lines;
    Vec2 m_bounds = VEC2_ZERO;
    TextBlockWidths m_widths;
    float m_lastWidth = -1.0f;
    const Font* m_resolvedFont = nullptr;
    uint m_fontVersion = 0;
    bool m_tagsClosed = true;
    bool m_spansDirty = true;
    bool m_widthsDirty = true;
    bool m_layoutDirty = true;

  public:
    TextBlock() = default;
    TextBlock(const Font* font, std::string_view text, const TextStyle& style = {});

    TextBlock(const TextBlock&) = delete;
    TextBlock& operator=(const TextBlock&) = delete;
    TextBlock(TextBlock&&) = delete;
    TextBlock& operator=(TextBlock&&) = delete;

    void setFont(const Font* font);
    void setText(std::string_view text);
    void setStyle(const TextStyle& style);
    void setSpanStyles(const std::vector<TextStyle>& spanStyles);
    void setAlignH(TextAlignH alignH);
    void setAlignV(TextAlignV alignV);
    void setOverflow(TextOverflow overflow);
    void setWrapEnabled(bool wrapEnabled);
    void setFixedLineHeight(bool fixedLineHeight);
    void invalidate();

    const Font* getFont() const { return m_font; }
    // The font this block was last solved against, which is the requested one only once
    // it has finished baking -- until then the layout is the default font's, and drawing
    // has to use the same one or the glyphs land at another face's positions.
    const Font* getResolvedFont() const { return m_resolvedFont ? m_resolvedFont : m_font; }
    const std::string& getText() const { return m_text; }
    const TextStyle& getStyle() const { return m_style; }
    const TextStyle& styleForRun(int styleIndex) const;
    TextAlignH getAlignH() const { return m_alignH; }
    TextAlignV getAlignV() const { return m_alignV; }
    TextOverflow getOverflow() const { return m_overflow; }
    bool isWrapEnabled() const { return m_wrapEnabled; }
    bool hasFixedLineHeight() const { return m_fixedLineHeight; }
    bool areTagsClosed() const { return m_tagsClosed; }

    const std::vector<TextRun>& getRuns() const { return m_runs; }
    const std::vector<TextLine>& getLines() const { return m_lines; }
    Vec2 getBounds() const { return m_bounds; }
    TextBlockWidths getWidths() const { return m_widths; }

    bool isDirty() const { return m_layoutDirty; }
    bool isDirtyFor(float availableWidth) const;

  private:
    void ensureSpans();
    void clearLayout();
};

}   // namespace Engine
