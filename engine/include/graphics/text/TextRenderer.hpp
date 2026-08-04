#pragma once

#include "graphics/BatchRenderer.hpp"
#include "graphics/text/Font.hpp"
#include "graphics/text/TextLayout.hpp"
#include "graphics/text/TextStyle.hpp"
#include "utils/Singleton.hpp"
#include "utils/math/Vec2.hpp"
#include <string_view>
#include <vector>

namespace Engine {

// unitRange is per-vertex, not a uniform, so glyphs from different fonts and styles
// stay in one batch. It also selects the shader's branch: negative is a solid fill
// (underline, strikethrough), zero is a coverage mask (bitmap atlas), positive is a
// distance field.
struct TextVertex
{
    Vec2 pos;
    Vec2 uv;
    Vec4 clipRect;   // minX, minY, maxX, maxY, in the same space as pos
    Color color;
    Color outlineColor;
    Color shadowColor;
    Vec2 unitRange;
    Vec2 shadowOffset;
    float shadowSoftness;
    float reserved;   // pads shadowOffset+shadowSoftness to a vec4 slot; unused
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
    // The font exposes no x-height, so the strikethrough rides a fraction of the
    // ascender instead.
    static constexpr float STRIKETHROUGH_ASCENDER_RATIO = 0.28f;
    static constexpr float SOLID_UNIT_RANGE = -1.0f;
    // Far enough out that no real geometry reaches it, so unclipped text needs no
    // branch -- the shader always tests the rect, it just always passes.
    static constexpr float NO_CLIP_EXTENT = 1e30f;

    BatchRenderer<TextVertex> m_batch;
    Mat4 m_viewProjOverride;
    Vec4 m_clipRect;
    bool m_hasViewProjOverride = false;
    bool m_initialized = false;
    IdType m_defaultShaderId = INVALID_ID;
    bool m_warnedDirtyBlock = false;

  public:
    // A null shader takes the engine's own, which is what almost every caller wants;
    // pass one only to render the glyph batch through something else.
    void init(GlShader* shader = nullptr, size_t maxQuadCount = 4000);
    void setShader(GlShader* shader);

    void setViewProjOverride(const Mat4& viewProj);
    void clearViewProjOverride();

    // Bounds every glyph emitted from here on, in the space the text is drawn in.
    // Unlike the view-proj override this needs no flush: the rect is per-vertex, so
    // text under different clips still shares one draw call.
    void setClipRect(const Vec4& clipRect);
    void clearClipRect();

    Vec2 draw(const TextBlock& block, Vec2 origin, Vec2 boxSize = VEC2_ZERO);

    Vec2 draw(
        const Font& font, std::string_view text, Vec2 origin, const TextStyle& style = {},
        const std::vector<TextStyle>& spanStyles = {}
    );

    Vec2 drawSpan(
        const Font& requested, std::string_view text, const TextStyle& style, Vec2 pen,
        float lineOriginX
    );

    void flush();

  private:
    TextRenderer();
    ~TextRenderer() = default;

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
    void appendGlyph(const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen);
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
};

}   // namespace Engine
