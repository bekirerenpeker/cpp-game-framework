#include "graphics/text/TextRenderer.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "graphics/text/FontLoader.hpp"
#include "graphics/text/TextLayoutCalculator.hpp"
#include "graphics/text/TextMetrics.hpp"
#include "utils/Utf8.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

TextRenderer::TextRenderer()
    : m_clipRect(-NO_CLIP_EXTENT, -NO_CLIP_EXTENT, NO_CLIP_EXTENT, NO_CLIP_EXTENT)
{
}

void TextRenderer::setClipRect(const Vec4& clipRect) { m_clipRect = clipRect; }

void TextRenderer::clearClipRect()
{
    m_clipRect = Vec4(-NO_CLIP_EXTENT, -NO_CLIP_EXTENT, NO_CLIP_EXTENT, NO_CLIP_EXTENT);
}

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

// Text drawn in a different space cannot share a draw call with text already queued,
// so switching flushes. Submit all world text, then all screen text.
void TextRenderer::setViewProjOverride(const Mat4& viewProj)
{
    if (m_initialized) flush();
    m_viewProjOverride = viewProj;
    m_hasViewProjOverride = true;
}

void TextRenderer::clearViewProjOverride()
{
    if (!m_hasViewProjOverride) return;
    if (m_initialized) flush();
    m_hasViewProjOverride = false;
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
    m_batch.setViewProjMat(
        m_hasViewProjOverride ? m_viewProjOverride : ViewContext::get().getViewProjMat()
    );
    return true;
}

// The solved layout is Y-down from the block's top-left, while the pen is Y-up, so
// subtracting run.offset.y is the single flip in the whole text path.
Vec2 TextRenderer::draw(const TextBlock& block, Vec2 origin, Vec2 boxSize)
{
    // The one the layout was solved against, not the one the caller named: while a font
    // bakes its block is laid out with the default font's metrics, and drawing the real
    // glyphs at those positions would be a different face on someone else's layout.
    const Font* font = block.getResolvedFont();
    if (!font || !font->isValid()) {
        LOG_WARNING("drawing a TextBlock with an invalid font; skipping");
        return VEC2_ZERO;
    }

    if (block.isDirty()) {
        if (!m_warnedDirtyBlock) {
            m_warnedDirtyBlock = true;
            LOG_WARNING(
                "drawing a TextBlock whose layout was never calculated; call "
                "TextLayoutCalculator::calculate first (further occurrences stay silent)"
            );
        }
        return VEC2_ZERO;
    }

    if (!ensureReady()) return block.getBounds();

    // Vertical alignment lands here rather than in the solver because it needs the
    // box height, which only the caller knows. A zero box means no slack, so
    // world-space text with no box behaves as Top.
    float slack = Math::max(0.0f, boxSize.y - block.getBounds().y);
    float yOffset = 0.0f;
    if (block.getAlignV() == TextAlignV::Middle) yOffset = slack * 0.5f;
    else if (block.getAlignV() == TextAlignV::Bottom) yOffset = slack;

    for (const TextRun& run : block.getRuns()) {
        if (run.text.empty()) continue;

        Vec2 pen(origin.x + run.offset.x, origin.y - yOffset - run.offset.y);
        drawSpan(*font, run.text, block.styleForRun(run.styleIndex), pen, pen.x);
    }
    return block.getBounds();
}

// The convenience form: everything here is per-call scratch, so a real frame loop
// should own a TextBlock instead of paying for a parse and a wrap every frame.
Vec2 TextRenderer::draw(
    const Font& font, std::string_view text, Vec2 origin, const TextStyle& style,
    const std::vector<TextStyle>& spanStyles
)
{
    TextBlock block(&font, text, style);
    block.setSpanStyles(spanStyles);
    TextLayoutCalculator::get().calculate(block, 0.0f);
    return draw(block, origin);
}

// A '\n' here steps by this span's style alone, which is the last remnant of the
// old baseline-to-baseline rule -- anything multi-line or multi-style belongs in a
// TextBlock, where a line is as tall as the tallest style touching it.
Vec2 TextRenderer::drawSpan(
    const Font& requested, std::string_view text, const TextStyle& style, Vec2 pen,
    float lineOriginX
)
{
    // Resolved here too, since this is the chaining primitive a caller can reach for
    // without a TextBlock -- it measures and emits in one pass, so both halves see the
    // same font either way.
    const Font& font = *FontLoader::get().resolve(&requested);

    if (!font.isValid() || text.empty()) return pen;
    if (!ensureReady()) return pen;

    const float spanStep = TextMetrics::lineStep(font, style);
    const bool decorated = style.underline || style.strikethrough;

    float decorationStartX = pen.x;
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
            if (decorated) appendLineDecorations(font, style, decorationStartX, pen.x, pen.y);
            pen.x = lineOriginX;
            pen.y -= spanStep;
            decorationStartX = pen.x;
            prev = 0;
            continue;
        }

        GlyphStep glyphStep = TextMetrics::step(font, style, codepoint, prev);

        // Kerning belongs to the pair, so it moves the pen before this glyph is
        // placed; folding it into the advance would draw every glyph one pair late.
        pen.x += glyphStep.kerning;
        if (glyphStep.glyph) appendGlyph(font, *glyphStep.glyph, style, pen);
        pen.x += glyphStep.advance;
        prev = glyphStep.kerningPrev;
    }

    if (decorated) appendLineDecorations(font, style, decorationStartX, pen.x, pen.y);
    return pen;
}

void TextRenderer::appendGlyph(
    const Font& font, const Glyph& glyph, const TextStyle& style, Vec2 pen
)
{
    // Whitespace carries an advance but no box, so it must not burn a quad slot.
    bool hasShape = glyph.quadMin.x != glyph.quadMax.x && glyph.quadMin.y != glyph.quadMax.y;
    if (!hasShape) return;

    if (font.getAtlasType() == FontAtlasType::Mtsdf) appendGlyphMtsdf(font, glyph, style, pen);
    else appendGlyphBitmap(font, glyph, style, pen);
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
            m_clipRect,
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
