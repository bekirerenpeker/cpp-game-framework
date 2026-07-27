#include "graphics/text/TextRenderer.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "utils/Utf8.hpp"

namespace Engine {

void TextRenderer::init(GlShader* shader, size_t maxQuadCount)
{
    m_batch.init(
        maxQuadCount,
        {
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {  GlDataType::Int, 1},
    },
        shader
    );
    m_initialized = true;
}

bool TextRenderer::ensureReady()
{
    if (!m_initialized) {
        LOG_WARNING("TextRenderer used before init(); skipping");
        return false;
    }

    Window* context = ViewContext::get().getActiveWindow();
    if (!context) {
        LOG_WARNING("TextRenderer used with no active window; skipping");
        return false;
    }

    // VAOs are not shared across GL contexts, so use this context's own, created
    // and configured the first time we draw into this window.
    GlVertexArray*& vao = context->vertexArray(this);
    if (!vao) {
        vao = new GlVertexArray();
        m_batch.configureVao(*vao);
    }
    m_batch.setVao(vao);
    m_batch.setViewProjMat(ViewContext::get().getViewProjMat());
    return true;
}

// Warns at most once for the whole run: draw and measure both parse, every frame,
// so warning inside the parser itself would bury the log under one line per frame.
void TextRenderer::parseSpans(std::string_view text)
{
    if (TextTags::parse(text, m_spans)) return;
    if (m_warnedUnclosedTag) return;

    m_warnedUnclosedTag = true;
    LOG_WARNING(
        "text has an unclosed '{}{}' tag, so its last span runs to the end of the string: \"{}\" "
        "(further occurrences stay silent)",
        TextTags::TAG_PREFIX, TextTags::TAG_STYLE, text
    );
}

const TextStyle& TextRenderer::pickStyle(
    const TextSpan& span, const TextStyle& style, const std::vector<TextStyle>& spanStyles
)
{
    // A style list shorter than the number of tagged spans is not an error -- the
    // extras just fall back to the default.
    if (span.styleIndex < 0 || (size_t)span.styleIndex >= spanStyles.size()) return style;
    return spanStyles[span.styleIndex];
}

// The top-left origin has to clear the tallest text on the first line, not just the
// default style's ascender, or a big span would poke out above the block.
float TextRenderer::firstLineSize(
    const std::vector<TextSpan>& spans, const TextStyle& style,
    const std::vector<TextStyle>& spanStyles
)
{
    float largest = style.size;
    for (const TextSpan& span : spans) {
        size_t newline = span.text.find('\n');
        std::string_view onFirstLine =
            span.text.substr(0, newline == std::string_view::npos ? span.text.size() : newline);

        const TextStyle& spanStyle = pickStyle(span, style, spanStyles);
        if (!onFirstLine.empty() && spanStyle.size > largest) largest = spanStyle.size;

        if (newline != std::string_view::npos) break;
    }
    return largest;
}

Vec2 TextRenderer::draw(
    const Font& font, std::string_view text, Vec2 origin, const TextStyle& style,
    const std::vector<TextStyle>& spanStyles
)
{
    if (!font.isValid()) {
        LOG_WARNING("drawing text with an invalid font; skipping");
        return origin;
    }

    parseSpans(text);

    // origin is the top-left of the first line's ascender box; everything below
    // this point works on the baseline pen instead.
    Vec2 pen(origin.x, origin.y - font.getAscender(firstLineSize(m_spans, style, spanStyles)));

    for (const TextSpan& span : m_spans) {
        pen = drawSpan(font, span.text, pickStyle(span, style, spanStyles), pen, origin.x);
    }
    return pen;
}

Vec2 TextRenderer::drawSpan(
    const Font& font, std::string_view text, const TextStyle& style, Vec2 pen, float lineOriginX
)
{
    if (!font.isValid() || text.empty()) return pen;
    if (!ensureReady()) return pen;

    TextPen state;
    state.pos = pen;
    walkSpan(font, text, style, state, lineOriginX, true);
    return state.pos;
}

Vec2 TextRenderer::measure(
    const Font& font, std::string_view text, const TextStyle& style,
    const std::vector<TextStyle>& spanStyles
)
{
    if (!font.isValid()) return VEC2_ZERO;

    TextPen state;
    parseSpans(text);
    for (const TextSpan& span : m_spans) {
        walkSpan(font, span.text, pickStyle(span, style, spanStyles), state, 0.0f, false);
    }

    float width = state.maxX > state.pos.x ? state.maxX : state.pos.x;
    // pos.y only ever moves down from the starting baseline, so its magnitude is
    // the stacked line advance; the last line's own box is added on top, sized by
    // the largest style that landed on it.
    float lastLineSize = state.maxLineSize > 0.0f ? state.maxLineSize : style.size;
    float lastLineBox = font.getAscender(lastLineSize) - font.getDescender(lastLineSize);
    return Vec2(width, lastLineBox - state.pos.y);
}

void TextRenderer::walkSpan(
    const Font& font, std::string_view text, const TextStyle& style, TextPen& pen,
    float lineOriginX, bool emit
)
{
    const float spanStep = font.getLineHeight(style.size) * style.lineSpacing;
    const bool decorated = style.underline || style.strikethrough;

    // A line is as tall as the tallest style on it, so the step is accumulated
    // across every span that touches the line and only consumed at the newline --
    // stepping by whichever span happens to hold the '\n' would let a big span on
    // one line collide with the next.
    if (spanStep > pen.maxLineStep) pen.maxLineStep = spanStep;
    if (style.size > pen.maxLineSize) pen.maxLineSize = style.size;

    float decorationStartX = pen.pos.x;
    uint32_t prev = 0;

    size_t i = 0;
    while (i < text.size()) {
        // "//s" is a literal "/s": drop the leading slash and let the next two
        // characters render as themselves. Uses the same scan order as the parser
        // so the two always agree on what is an escape.
        if (TextTags::isEscapedTagAt(text, i)) {
            i++;
            continue;
        }

        uint32_t codepoint = Utf8::next(text, i);
        if (codepoint == 0) break;

        if (codepoint == '\r') continue;

        if (codepoint == '\n') {
            if (emit && decorated) {
                appendLineDecorations(font, style, decorationStartX, pen.pos.x, pen.pos.y);
            }
            if (pen.pos.x > pen.maxX) pen.maxX = pen.pos.x;

            pen.pos.x = lineOriginX;
            pen.pos.y -= pen.maxLineStep;
            // The rest of this span continues on the new line, so it seeds the next
            // line's height instead of starting from nothing.
            pen.maxLineStep = spanStep;
            pen.maxLineSize = style.size;
            decorationStartX = pen.pos.x;
            prev = 0;
            continue;
        }

        if (codepoint == '\t') {
            const Glyph* space = font.getGlyph(' ');
            float advance = space ? space->advance : FALLBACK_SPACE_ADVANCE;
            pen.pos.x += (advance * TAB_SPACES + style.letterSpacing) * style.size;
            prev = 0;
            continue;
        }

        if (prev != 0) pen.pos.x += font.getKerning(prev, codepoint) * style.size;

        const Glyph* glyph = resolveGlyph(font, codepoint);
        if (!glyph) {
            prev = 0;
            continue;
        }

        if (emit) pen.pos = drawGlyph(font, *glyph, style, pen.pos);
        else pen.pos.x += (glyph->advance + style.letterSpacing) * style.size;

        prev = codepoint;
    }

    if (emit && decorated) {
        appendLineDecorations(font, style, decorationStartX, pen.pos.x, pen.pos.y);
    }
    if (pen.pos.x > pen.maxX) pen.maxX = pen.pos.x;
}

const Glyph* TextRenderer::resolveGlyph(const Font& font, uint32_t codepoint)
{
    const Glyph* glyph = font.getGlyph(codepoint);
    if (glyph) return glyph;

    if (!m_warnedMissingGlyph) {
        m_warnedMissingGlyph = true;
        LOG_WARNING(
            "font {} has no glyph for codepoint {}; substituting '?' where it exists "
            "(further misses stay silent)",
            font.getSourcePath(), codepoint
        );
    }
    return font.getGlyph('?');
}

Vec2 TextRenderer::drawGlyph(const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen)
{
    // Whitespace carries an advance but no box, so it must not burn a quad slot.
    bool hasShape = glyph.quadMin.x != glyph.quadMax.x && glyph.quadMin.y != glyph.quadMax.y;
    if (hasShape) {
        if (font.getAtlasType() == FontAtlasType::Mtsdf) appendGlyphMtsdf(font, glyph, style, pen);
        else appendGlyphBitmap(font, glyph, style, pen);
    }

    return Vec2(pen.x + (glyph.advance + style.letterSpacing) * style.size, pen.y);
}

void TextRenderer::appendGlyphMtsdf(
    const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen
)
{
    GlyphAppearance appearance;
    appearance.color = style.color;
    appearance.outlineColor = style.outlineColor;
    appearance.shadowColor = style.shadowColor;
    appearance.unitRange = font.getUnitRange();

    // Widths arrive in em but the shader works in normalized field units, where 1.0
    // spans the whole baked distance range. Converting here rather than passing
    // pixels is what keeps an outline, a weight or a glow proportional to the text
    // instead of drifting as the camera zooms.
    float distanceRange = font.getDistanceRange();
    float emToField = distanceRange > 0.0f ? font.getNativePixelSize() / distanceRange : 0.0f;
    appearance.outlineWidth = style.outlineWidth * emToField;
    appearance.boldness = style.boldness * emToField;
    appearance.softness = style.softness * emToField;
    appearance.shadowWidth = style.shadowWidth * emToField;
    appearance.shadowSoftness = style.shadowSoftness * emToField;

    // The shadow is produced by sampling the field at an offset, so the em offset
    // has to become an atlas-UV offset. One em spans emPixelSize atlas pixels, and
    // the sample moves opposite to the direction the shadow should appear in.
    Vec2 atlasSize = font.getAtlasSize();
    if (atlasSize.x > 0 && atlasSize.y > 0) {
        Vec2 uvPerEm(
            font.getNativePixelSize() / atlasSize.x, font.getNativePixelSize() / atlasSize.y
        );
        appearance.shadowOffset =
            Vec2(-style.shadowOffset.x * uvPerEm.x, -style.shadowOffset.y * uvPerEm.y);
    }

    writeQuad(
        font.getTexture(), pen + glyph.quadMin * style.size, pen + glyph.quadMax * style.size,
        glyph.uvMin, glyph.uvMax, pen.y, style.italicSkew, appearance
    );
}

void TextRenderer::appendGlyphBitmap(
    const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen
)
{
    // A zero unitRange puts the fragment shader on its plain-coverage branch, which
    // is what makes every distance-field field below a no-op -- the caller can set
    // an outline or a shadow and this font type simply ignores them.
    GlyphAppearance appearance;
    appearance.color = style.color;

    writeQuad(
        font.getTexture(), pen + glyph.quadMin * style.size, pen + glyph.quadMax * style.size,
        glyph.uvMin, glyph.uvMax, pen.y, style.italicSkew, appearance
    );
}

// Underline and strikethrough are geometry, not shading, so they work identically
// for both atlas types. Emitted once per line segment rather than per glyph.
void TextRenderer::appendLineDecorations(
    const Font& font, const TextStyle& style, float startX, float endX, float baselineY
)
{
    if (endX <= startX) return;

    const FontMetrics& metrics = font.getMetrics();
    float thickness = metrics.underlineThickness * style.size;
    if (thickness <= 0.0f) return;

    if (style.underline) {
        float y = baselineY + metrics.underlineY * style.size;
        appendSolidQuad(
            font.getTexture(), Vec2(startX, y - thickness * 0.5f), Vec2(endX, y + thickness * 0.5f),
            style.color, baselineY, style.italicSkew
        );
    }
    if (style.strikethrough) {
        float y = baselineY + metrics.ascender * STRIKETHROUGH_ASCENDER_RATIO * style.size;
        appendSolidQuad(
            font.getTexture(), Vec2(startX, y - thickness * 0.5f), Vec2(endX, y + thickness * 0.5f),
            style.color, baselineY, style.italicSkew
        );
    }
}

// A negative unitRange tells the shader to ignore the atlas and fill flat. The
// font's own texture is still handed to the batch: it needs a non-null pointer for
// slot assignment, and reusing a texture already in the batch costs no extra slot.
void TextRenderer::appendSolidQuad(
    const GlTexture* texture, Vec2 min, Vec2 max, Color color, float baselineY, float italicSkew
)
{
    GlyphAppearance appearance;
    appearance.color = color;
    appearance.unitRange = Vec2(SOLID_UNIT_RANGE, SOLID_UNIT_RANGE);

    writeQuad(texture, min, max, VEC2_ZERO, VEC2_ONE, baselineY, italicSkew, appearance);
}

void TextRenderer::writeQuad(
    const GlTexture* texture, Vec2 min, Vec2 max, Vec2 uvMin, Vec2 uvMax, float baselineY,
    float italicSkew, const GlyphAppearance& appearance
)
{
    // Corner and UV order matches Renderer::addQuad so the shared 0,1,2 0,2,3
    // index buffer winds correctly: bottom-left, bottom-right, top-right, top-left.
    Vec2 corners[4] = {
        Vec2(min.x, min.y), Vec2(max.x, min.y), Vec2(max.x, max.y), Vec2(min.x, max.y)
    };
    const Vec2 uvs[4] = {
        Vec2(uvMin.x, uvMin.y), Vec2(uvMax.x, uvMin.y), Vec2(uvMax.x, uvMax.y),
        Vec2(uvMin.x, uvMax.y)
    };

    // Italics are a pure shear about the baseline, so they cost nothing in the
    // shader and apply to decorations and bitmap glyphs just the same.
    if (italicSkew != 0.0f) {
        for (Vec2& corner : corners) corner.x += (corner.y - baselineY) * italicSkew;
    }

    BatchRenderer<TextVertex>::Quad quad = m_batch.nextQuad(texture);
    for (int i = 0; i < 4; i++) {
        quad.verts[i] = {
            corners[i],
            uvs[i],
            appearance.color,
            appearance.outlineColor,
            appearance.shadowColor,
            appearance.unitRange,
            appearance.shadowOffset,
            appearance.shadowSoftness,
            0.0f,
            appearance.outlineWidth,
            appearance.boldness,
            appearance.softness,
            appearance.shadowWidth,
            quad.texIndex
        };
    }
}

void TextRenderer::flush()
{
    if (!ensureReady()) return;
    m_batch.flush();
}

}   // namespace Engine
