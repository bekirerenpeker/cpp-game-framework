#pragma once

#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/text/TextTags.hpp"
#include "graphics/ui/ILeafMeasurer.hpp"
#include "graphics/ui/TextMeasure.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace Engine {

class GlTexture;

class TextLeaf : public ILeafMeasurer
{
  private:
    const Font* m_font = nullptr;
    std::string m_text;
    TextStyle m_style;
    std::vector<TextStyle> m_spanStyles;
    std::vector<TextSpan> m_spans;
    std::vector<TextRun> m_runs;
    std::vector<LayoutLine> m_lines;
    bool m_wrapEnabled = true;
    bool m_fixedLineHeight = false;
    bool m_tagsClosed = true;

  public:
    void
    set(const Font* font, std::string_view text, const TextStyle& style,
        const std::vector<TextStyle>& spanStyles, bool wrapEnabled, bool fixedLineHeight);

    const Font* getFont() const { return m_font; }
    const std::string& getText() const { return m_text; }
    const TextStyle& getStyle() const { return m_style; }
    const std::vector<TextStyle>& getSpanStyles() const { return m_spanStyles; }
    const std::vector<TextRun>& getRuns() const { return m_runs; }
    const std::vector<LayoutLine>& getLines() const { return m_lines; }
    bool areTagsClosed() const { return m_tagsClosed; }

    const TextStyle& styleFor(int styleIndex) const;

    LeafWidths measureWidths() override;
    float measureHeight(float contentWidth, std::vector<LayoutLine>& lines) override;
};

class ImageLeaf : public ILeafMeasurer
{
  private:
    const GlTexture* m_texture = nullptr;
    Vec2 m_nativeSize = VEC2_ZERO;
    float m_aspectRatio = 0.0f;

  public:
    void set(const GlTexture* texture, Vec2 nativeSize, float aspectRatio);

    const GlTexture* getTexture() const { return m_texture; }
    Vec2 getNativeSize() const { return m_nativeSize; }

    LeafWidths measureWidths() override;
    float measureHeight(float contentWidth, std::vector<LayoutLine>& lines) override;
};

}   // namespace Engine
