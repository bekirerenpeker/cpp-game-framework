#include "graphics/ui/UiRenderer.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "graphics/text/TextRenderer.hpp"
#include "graphics/ui/elements/UiLeaves.hpp"

namespace Engine {

void UiRenderer::init(GlShader* shader, size_t maxQuadCount)
{
    m_batch.init(
        maxQuadCount,
        {
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 1},
            {GlDataType::Float, 1},
    },
        shader
    );
    m_initialized = true;
}

void UiRenderer::setViewport(Vec2 screenTopLeft, float pixelsPerUiUnit)
{
    m_screenTopLeft = screenTopLeft;
    m_pixelsPerUiUnit = pixelsPerUiUnit;
}

Vec2 UiRenderer::uiToScreen(Vec2 uiPos) const
{
    // The only Y flip in the subsystem: layout is Y-down from a top-left origin, the
    // GL viewport is Y-up from a bottom-left one. Text is drawn baseline-up, so it is
    // the projection that has to stay Y-up rather than the layout that gets flipped.
    return Vec2(
        m_screenTopLeft.x + uiPos.x * m_pixelsPerUiUnit,
        m_windowHeight - (m_screenTopLeft.y + uiPos.y * m_pixelsPerUiUnit)
    );
}

bool UiRenderer::ensureReady()
{
    if (!m_initialized) {
        LOG_WARNING("UiRenderer used before init(); skipping");
        return false;
    }

    Window* context = ViewContext::get().getActiveWindow();
    if (!context) {
        LOG_WARNING("UiRenderer used with no active window; skipping");
        return false;
    }

    float width = (float)context->getWidth();
    m_windowHeight = (float)context->getHeight();
    if (width <= 0.0f || m_windowHeight <= 0.0f) return false;

    // VAOs are not shared across GL contexts, so use this context's own, created and
    // configured the first time we draw into this window.
    GlVertexArray*& vao = context->vertexArray(this);
    if (!vao) {
        vao = new GlVertexArray();
        m_batch.configureVao(*vao);
    }
    m_batch.setVao(vao);

    m_viewProjMat = Mat4::ortho(0.0f, width, 0.0f, m_windowHeight, -1.0f, 1.0f);
    m_batch.setViewProjMat(m_viewProjMat);
    return true;
}

void UiRenderer::render(
    const std::vector<UiElement>& elements, uint elementCount, const std::vector<LayoutNode>& nodes
)
{
    if (nodes.empty() || !ensureReady()) return;

    renderRects(elements, elementCount, nodes);
    // Rects and glyphs are separate batches, so this has to resolve before any glyph is
    // submitted or the two draw in whichever order they happen to flush.
    m_batch.flush();

    if (m_drawText) renderText(elements, elementCount, nodes);
}

void UiRenderer::flush()
{
    if (!ensureReady()) return;
    m_batch.flush();
}

void UiRenderer::renderRects(
    const std::vector<UiElement>& elements, uint elementCount, const std::vector<LayoutNode>& nodes
)
{
    // Preorder, so a child's rect lands on top of its parent's with no depth sorting --
    // the same order the solver and the hit test use.
    for (uint i = 0; i < elementCount && i < nodes.size(); i++)
        addRect(nodes[i].pos, nodes[i].size, elements[i].style);
}

void UiRenderer::addRect(Vec2 uiMin, Vec2 uiSize, const UiStyle& style)
{
    if (uiSize.x <= 0.0f || uiSize.y <= 0.0f) return;

    bool hasFill = style.backgroundColor.a > 0.0f;
    bool hasBorder = style.borderWidth > 0.0f && style.borderColor.a > 0.0f;
    // A container with neither is pure layout scaffolding, which is most of them --
    // it must cost no geometry at all, not a transparent quad.
    if (!hasFill && !hasBorder) return;

    Vec2 halfSize = uiSize * 0.5f * m_pixelsPerUiUnit;
    Vec2 center = uiToScreen(uiMin + uiSize * 0.5f);

    auto quad = m_batch.nextQuad();
    const Vec2 corners[4] = {
        Vec2(-halfSize.x, -halfSize.y), Vec2(halfSize.x, -halfSize.y), Vec2(halfSize.x, halfSize.y),
        Vec2(-halfSize.x, halfSize.y)
    };

    for (int i = 0; i < 4; i++) {
        UiVertex& vertex = quad.verts[i];
        vertex.pos = center + corners[i];
        vertex.localPos = corners[i];
        vertex.halfSize = halfSize;
        vertex.fillColor = hasFill ? style.backgroundColor : COLOR_CLEAR;
        vertex.borderColor = hasBorder ? style.borderColor : COLOR_CLEAR;
        vertex.cornerRadius = style.cornerRadius * m_pixelsPerUiUnit;
        vertex.borderWidth = hasBorder ? style.borderWidth * m_pixelsPerUiUnit : 0.0f;
    }
}

void UiRenderer::renderText(
    const std::vector<UiElement>& elements, uint elementCount, const std::vector<LayoutNode>& nodes
)
{
    TextRenderer::get().setViewProjOverride(m_viewProjMat);

    for (uint i = 0; i < elementCount && i < nodes.size(); i++) {
        const UiElement& element = elements[i];
        if (element.type != UiElementType::Text || !element.measurer) continue;

        const TextLeaf* leaf = static_cast<const TextLeaf*>(element.measurer);
        const Font* font = leaf->getFont();
        if (!font || !font->isValid()) continue;

        Vec2 contentTopLeft = nodes[i].pos + element.layout.padding.topLeft();

        for (const TextRun& run : leaf->getRuns()) {
            if (run.text.empty()) continue;

            // Every TextStyle measurement except size is in em, so scaling size is the
            // whole conversion from layout units into pixels.
            TextStyle style = leaf->styleFor(run.styleIndex);
            style.size *= m_pixelsPerUiUnit;

            Vec2 pen = uiToScreen(contentTopLeft + run.offset);
            TextRenderer::get().drawSpan(*font, run.text, style, pen, pen.x);
        }
    }

    TextRenderer::get().flush();
    TextRenderer::get().clearViewProjOverride();
}

}   // namespace Engine
