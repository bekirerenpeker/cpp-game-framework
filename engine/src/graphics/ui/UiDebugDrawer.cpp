#include "graphics/ui/UiDebugDrawer.hpp"
#include "graphics/Renderer.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"

namespace Engine {

static const Color DEPTH_COLORS[] = {
    Color(0.45f, 0.72f, 1.00f), Color(0.55f, 0.92f, 0.60f), Color(1.00f, 0.80f, 0.35f),
    Color(0.95f, 0.55f, 0.80f), Color(0.60f, 0.65f, 1.00f), Color(0.50f, 0.92f, 0.90f),
};
static constexpr uint DEPTH_COLOR_COUNT = sizeof(DEPTH_COLORS) / sizeof(DEPTH_COLORS[0]);

static const Color FLOATING_COLOR = Color(1.00f, 0.35f, 0.35f);

void UiDebugDrawer::setViewport(Vec2 worldTopLeft, float worldUnitsPerUiUnit)
{
    m_worldTopLeft = worldTopLeft;
    m_worldUnitsPerUiUnit = worldUnitsPerUiUnit;
}

Vec2 UiDebugDrawer::uiToWorld(Vec2 uiPos) const
{
    // The only Y flip in the subsystem: layout is Y-down from a top-left origin, the
    // engine's world is Y-up.
    return m_worldTopLeft + Vec2(uiPos.x, -uiPos.y) * m_worldUnitsPerUiUnit;
}

void UiDebugDrawer::draw(const std::vector<LayoutNode>& nodes)
{
    if (nodes.empty()) return;
    ensureTexture();

    for (const LayoutNode& node : nodes) {
        Color color = node.isFloating ? FLOATING_COLOR : depthColor(node.depth);
        if (m_drawFill) {
            fillRect(node.pos, node.size, Color(color.r, color.g, color.b, m_fillAlpha));
        }
        outlineRect(node.pos, node.size, color);
    }
}

void UiDebugDrawer::release()
{
    delete m_whiteTexture;
    m_whiteTexture = nullptr;
}

void UiDebugDrawer::ensureTexture()
{
    // Built on first use rather than in a constructor: this lives inside a Singleton,
    // whose instance can exist before any GL context does.
    if (!m_whiteTexture) m_whiteTexture = new GlTexture(COLOR_WHITE);
}

void UiDebugDrawer::fillRect(Vec2 uiMin, Vec2 uiSize, Color color)
{
    if (uiSize.x <= 0.0f || uiSize.y <= 0.0f) return;

    // addQuad takes a centre and a full size, not a min/max pair.
    Vec2 center = uiToWorld(uiMin + uiSize * 0.5f);
    Renderer::get().addQuad(center, uiSize * m_worldUnitsPerUiUnit, color, m_whiteTexture);
}

void UiDebugDrawer::outlineRect(Vec2 uiMin, Vec2 uiSize, Color color)
{
    if (uiSize.x <= 0.0f || uiSize.y <= 0.0f) return;

    float thickness = m_outlineThickness;
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

Color UiDebugDrawer::depthColor(uint depth) { return DEPTH_COLORS[depth % DEPTH_COLOR_COUNT]; }

}   // namespace Engine
