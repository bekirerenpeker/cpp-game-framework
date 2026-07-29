#include "graphics/ui/UiRenderer.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "graphics/text/TextRenderer.hpp"
#include "graphics/ui/elements/UiLeaves.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

static const Color DEPTH_COLORS[] = {
    Color(0.45f, 0.72f, 1.00f), Color(0.55f, 0.92f, 0.60f), Color(1.00f, 0.80f, 0.35f),
    Color(0.95f, 0.55f, 0.80f), Color(0.60f, 0.65f, 1.00f), Color(0.50f, 0.92f, 0.90f),
};
static constexpr uint DEPTH_COLOR_COUNT = sizeof(DEPTH_COLORS) / sizeof(DEPTH_COLORS[0]);

static const Color FLOATING_COLOR = Color(1.00f, 0.35f, 0.35f);

void UiRenderer::setViewport(Vec2 worldTopLeft, float worldPerUiUnit)
{
    m_worldTopLeft = worldTopLeft;
    m_worldPerUiUnit = worldPerUiUnit;
}

Vec2 UiRenderer::uiToWorld(Vec2 uiPos) const
{
    // The only Y flip in the subsystem: layout is Y-down from a top-left origin,
    // the engine's world is Y-up.
    return m_worldTopLeft + Vec2(uiPos.x, -uiPos.y) * m_worldPerUiUnit;
}

Vec2 UiRenderer::worldToUi(Vec2 worldPos) const
{
    if (m_worldPerUiUnit == 0.0f) return VEC2_ZERO;

    Vec2 offset = (worldPos - m_worldTopLeft) / m_worldPerUiUnit;
    return Vec2(offset.x, -offset.y);
}

void UiRenderer::render(
    const std::vector<UiElement>& elements, uint elementCount, const std::vector<LayoutNode>& nodes
)
{
    if (nodes.empty()) return;
    ensureTexture();

    if (m_drawStyles) renderStyles(elements, elementCount, nodes);
    if (m_drawBoxes) renderDebugBoxes(nodes);

    // The rects go through the sprite batch and the glyphs through the text batch, so
    // the first has to be flushed or the two resolve in whichever order they happen to
    // flush. Unconditional: styles alone still fill that batch.
    Renderer::get().endScene();

    if (m_drawText) {
        renderText(elements, elementCount, nodes);
        TextRenderer::get().flush();
    }
}

void UiRenderer::release()
{
    delete m_whiteTexture;
    m_whiteTexture = nullptr;
}

void UiRenderer::ensureTexture()
{
    // Built on first use rather than in a constructor: this lives inside a Singleton,
    // whose instance can exist before any GL context does.
    if (!m_whiteTexture) m_whiteTexture = new GlTexture(COLOR_WHITE);
}

void UiRenderer::renderStyles(
    const std::vector<UiElement>& elements, uint elementCount, const std::vector<LayoutNode>& nodes
)
{
    // Preorder, so a child's background lands on top of its parent's without needing
    // any depth sorting -- the same order the debug boxes and the solver use.
    for (uint i = 0; i < elementCount && i < nodes.size(); i++) {
        const UiStyle& style = elements[i].style;
        const LayoutNode& node = nodes[i];

        if (style.backgroundColor.a > 0.0f) fillRect(node.pos, node.size, style.backgroundColor);
        if (style.borderWidth > 0.0f && style.borderColor.a > 0.0f)
            outlineRect(node.pos, node.size, style.borderColor, style.borderWidth);
    }
}

void UiRenderer::renderDebugBoxes(const std::vector<LayoutNode>& nodes)
{
    for (const LayoutNode& node : nodes) {
        Color color = node.isFloating ? FLOATING_COLOR : depthColor(node.depth);
        fillRect(node.pos, node.size, Color(color.r, color.g, color.b, m_fillAlpha));
        outlineRect(node.pos, node.size, color, m_outlineThickness);
    }
}

void UiRenderer::renderText(
    const std::vector<UiElement>& elements, uint elementCount, const std::vector<LayoutNode>& nodes
)
{
    for (uint i = 0; i < elementCount && i < nodes.size(); i++) {
        const UiElement& element = elements[i];
        if (element.type != UiElementType::Text || !element.measurer) continue;

        const TextLeaf* leaf = static_cast<const TextLeaf*>(element.measurer);
        const Font* font = leaf->getFont();
        if (!font || !font->isValid()) continue;

        Vec2 contentTopLeft = nodes[i].pos + element.layout.padding.topLeft();

        for (const TextRun& run : leaf->getRuns()) {
            if (run.text.empty()) continue;

            // Every TextStyle measurement except size is in em, so scaling size is
            // the whole conversion from layout units into world units.
            TextStyle style = leaf->styleFor(run.styleIndex);
            style.size *= m_worldPerUiUnit;

            Vec2 pen = uiToWorld(contentTopLeft + run.offset);
            TextRenderer::get().drawSpan(*font, run.text, style, pen, pen.x);
        }
    }
}

void UiRenderer::fillRect(Vec2 uiMin, Vec2 uiSize, Color color)
{
    if (uiSize.x <= 0.0f || uiSize.y <= 0.0f) return;

    // addQuad takes a centre and a full size, not a min/max pair.
    Vec2 center = uiToWorld(uiMin + uiSize * 0.5f);
    Renderer::get().addQuad(center, uiSize * m_worldPerUiUnit, color, m_whiteTexture);
}

void UiRenderer::outlineRect(Vec2 uiMin, Vec2 uiSize, Color color, float requestedThickness)
{
    if (uiSize.x <= 0.0f || uiSize.y <= 0.0f) return;

    float thickness = Math::min(requestedThickness, Math::min(uiSize.x, uiSize.y) * 0.5f);
    // Drawn inside the box so a child's border never sits on top of its parent's.
    fillRect(uiMin, Vec2(uiSize.x, thickness), color);
    fillRect(Vec2(uiMin.x, uiMin.y + uiSize.y - thickness), Vec2(uiSize.x, thickness), color);
    fillRect(
        Vec2(uiMin.x, uiMin.y + thickness), Vec2(thickness, uiSize.y - thickness * 2.0f), color
    );
    fillRect(
        Vec2(uiMin.x + uiSize.x - thickness, uiMin.y + thickness),
        Vec2(thickness, uiSize.y - thickness * 2.0f), color
    );
}

Color UiRenderer::depthColor(uint depth) { return DEPTH_COLORS[depth % DEPTH_COLOR_COUNT]; }

}   // namespace Engine
