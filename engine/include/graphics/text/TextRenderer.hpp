#pragma once

#include "graphics/BatchRenderer.hpp"
#include "graphics/text/Font.hpp"
#include "graphics/text/TextStyle.hpp"
#include "graphics/text/TextTags.hpp"
#include "utils/Singleton.hpp"
#include "utils/math/Vec2.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// unitRange is per-vertex, not a uniform, so glyphs from different fonts and
// different styles all stay in one batch. It also selects the shader's branch:
// negative is a solid untextured fill (underline, strikethrough, UI rects), zero
// is a plain coverage mask (bitmap atlas), positive is a distance field.
struct TextVertex
{
    Vec2 pos;
    Vec2 uv;
    Color color;
    Color outlineColor;
    Color shadowColor;
    Vec2 unitRange;
    Vec2 shadowOffset;
    float shadowSoftness;
    float reserved;
    float outlineWidth;
    float boldness;
    float softness;
    float shadowWidth;
    int texIndex;
};

class TextRenderer : public Singleton<TextRenderer>
{
    friend class Singleton<TextRenderer>;

  private:
    static constexpr float TAB_SPACES = 4.0f;
    static constexpr float FALLBACK_SPACE_ADVANCE = 0.25f;
    // The font exposes no x-height, so the strikethrough rides a fraction of the
    // ascender instead.
    static constexpr float STRIKETHROUGH_ASCENDER_RATIO = 0.28f;
    static constexpr float SOLID_UNIT_RANGE = -1.0f;

    BatchRenderer<TextVertex> m_batch;
    std::vector<TextSpan> m_spans;
    bool m_initialized = false;
    bool m_warnedMissingGlyph = false;
    bool m_warnedUnclosedTag = false;

  public:
    void init(GlShader* shader, size_t maxQuadCount = 4000);

    Vec2 draw(
        const Font& font, std::string_view text, Vec2 origin, const TextStyle& style = {},
        const std::vector<TextStyle>& spanStyles = {}
    );

    Vec2 drawSpan(
        const Font& font, std::string_view text, const TextStyle& style, Vec2 pen, float lineOriginX
    );

    Vec2 drawGlyph(const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen);

    Vec2 measure(
        const Font& font, std::string_view text, const TextStyle& style = {},
        const std::vector<TextStyle>& spanStyles = {}
    );

    void flush();

  private:
    TextRenderer() = default;
    ~TextRenderer() = default;

    struct TextPen
    {
        Vec2 pos = VEC2_ZERO;
        float maxX = 0.0f;
        float maxLineStep = 0.0f;
        float maxLineSize = 0.0f;
    };

    struct GlyphAppearance
    {
        Color color = COLOR_WHITE;
        Color outlineColor = COLOR_CLEAR;
        Color shadowColor = COLOR_CLEAR;
        Vec2 unitRange = VEC2_ZERO;
        Vec2 shadowOffset = VEC2_ZERO;
        float shadowSoftness = 0.0f;
        float outlineWidth = 0.0f;
        float boldness = 0.0f;
        float softness = 0.0f;
        float shadowWidth = 0.0f;
    };

    bool ensureReady();
    void parseSpans(std::string_view text);
    void walkSpan(
        const Font& font, std::string_view text, const TextStyle& style, TextPen& pen,
        float lineOriginX, bool emit
    );
    const Glyph* resolveGlyph(const Font& font, uint32_t codepoint);
    void appendGlyphMtsdf(const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen);
    void appendGlyphBitmap(const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen);
    void writeQuad(
        const GlTexture* texture, Vec2 min, Vec2 max, Vec2 uvMin, Vec2 uvMax, float baselineY,
        float italicSkew, const GlyphAppearance& appearance
    );
    void appendLineDecorations(
        const Font& font, const TextStyle& style, float startX, float endX, float baselineY
    );
    void appendSolidQuad(
        const GlTexture* texture, Vec2 min, Vec2 max, Color color, float baselineY, float italicSkew
    );
    static float firstLineSize(
        const std::vector<TextSpan>& spans, const TextStyle& style,
        const std::vector<TextStyle>& spanStyles
    );
    static const TextStyle& pickStyle(
        const TextSpan& span, const TextStyle& style, const std::vector<TextStyle>& spanStyles
    );
};

}   // namespace Engine
