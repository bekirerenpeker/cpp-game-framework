#include "graphics/ui/UIRenderer.hpp"
#include "core/input/Input.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/ViewContext.hpp"
#include "core/window_management/Window.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

void UIRenderer::init(GlShader* shader, size_t maxQuadCount)
{
    m_batch.init(
        maxQuadCount,
        {
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {GlDataType::Float, 4},
            {  GlDataType::Int, 1},
    },
        shader
    );
    m_initialized = true;
}

// Boxes drawn in a different space cannot share a draw call with boxes already
// queued, so switching flushes -- same rule as TextRenderer's view-proj override.
void UIRenderer::setSpace(UISpace space)
{
    if (m_space == space) return;
    // Guards more than the draw call: setSpace runs during setup too, and flushing an
    // empty batch there would build this window's VAO before the render context is
    // bound.
    if (m_initialized && m_batch.getQuadCount() > 0) flush();
    m_space = space;
}

// Screen space is deliberately Y-**up** (0,0 at the window's bottom-left): glyph
// quads are built baseline-up, so a Y-down matrix would render every string mirrored.
// The flip lives in getRootOrigin instead.
Mat4 UIRenderer::getViewProjMat() const
{
    if (m_space == UISpace::World) return ViewContext::get().getViewProjMat();

    Window* window = ViewContext::get().getActiveWindow();
    if (!window) return Mat4();
    return Mat4::ortho(
        0.0f, (float)window->getWidth(), 0.0f, (float)window->getHeight(), -1.0f, 1.0f
    );
}

// Where a root's top-left corner sits. Screen puts it in the window's top-left, which
// is the whole of the Y flip; world leaves it on the world origin, so the UI hangs
// down and to the right of it the way every other top-left anchored box does.
Vec2 UIRenderer::getRootOrigin() const
{
    if (m_space == UISpace::World) return VEC2_ZERO;

    Window* window = ViewContext::get().getActiveWindow();
    return window ? Vec2(0.0f, (float)window->getHeight()) : VEC2_ZERO;
}

// The mouse in whatever space the UI is currently solved in, so hit testing compares
// against a drawPos without either side knowing which space it is in.
Vec2 UIRenderer::getMouseUiPos() const
{
    if (m_space == UISpace::World) return ViewContext::get().getMouseWorldPos();

    Window* window = ViewContext::get().getActiveWindow();
    if (!window) return VEC2_ZERO;
    // Input reports window-centred pixels, +Y up; screen space is the same pixels
    // measured from the bottom-left corner.
    return Input::get().getMousePos() + Vec2(window->getWidth() * 0.5f, window->getHeight() * 0.5f);
}

bool UIRenderer::ensureReady()
{
    if (!m_initialized) {
        LOG_WARNING("UIRenderer used before init(); skipping");
        return false;
    }

    Window* context = ViewContext::get().getActiveWindow();
    if (!context) {
        LOG_WARNING("UIRenderer used with no active window; skipping");
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
    m_batch.setViewProjMat(getViewProjMat());
    return true;
}

void UIRenderer::addContainerQuad(
    Vec2 drawPos, Vec2 size, const UIContainerStyle& style, Vec4 clipRect
)
{
    if (size.x <= 0.0f || size.y <= 0.0f) return;
    // Fully clipped away, so the quad would produce no fragments at all.
    if (clipRect.z <= clipRect.x || clipRect.w <= clipRect.y) return;

    const GlTexture* image = style.backgroundImage.value_or(nullptr);
    // An image with no colour set draws untinted; a colour with no image is the plain
    // fill, since the batch resolves a null texture to its white default slot.
    Color fillColor = style.backgroundColor.value_or(image ? COLOR_WHITE : COLOR_CLEAR);
    Color borderColor = style.borderColor.value_or(COLOR_CLEAR);
    Color shadowColor = style.shadowColor.value_or(COLOR_CLEAR);

    Vec2 half = size * 0.5f;
    float halfMin = Math::min(half.x, half.y);
    // Past half the shorter side the radius folds the distance field inside out, and
    // a border thicker than that would cross the box's own centre.
    float radius = Math::clamp(style.borderRadius.value_or(0.0f), 0.0f, halfMin);
    float borderWidth =
        borderColor.a > 0.0f ? Math::clamp(style.borderWidth.value_or(0.0f), 0.0f, halfMin) : 0.0f;

    float shadowBlur = 0.0f;
    Vec2 shadowOffset = VEC2_ZERO;
    if (shadowColor.a > 0.0f) {
        shadowBlur = Math::max(style.shadowBlurRadius.value_or(0.0f), 0.0f);
        shadowOffset = style.shadowOffset.value_or(VEC2_ZERO);
        // The style names the offset the way CSS does, y growing downwards, while
        // every geometric field from here on is in the renderer's y-up draw space.
        shadowOffset.y = -shadowOffset.y;
    }

    // Nothing to paint: a layout-only container is the common case, and it costs no
    // geometry and no render-context work at all.
    if (fillColor.a <= 0.0f && borderWidth <= 0.0f && shadowColor.a <= 0.0f) return;
    if (!ensureReady()) return;

    float dashPeriod = 0.0f, dashRatio = 0.0f;
    UIBorderStyle borderStyle = style.borderStyle.value_or(UIBorderStyle::Solid);
    if (borderWidth > 0.0f && borderStyle != UIBorderStyle::Solid) {
        Vec2 straight(Math::max(half.x - radius, 0.0f), Math::max(half.y - radius, 0.0f));
        float perimeter = 4.0f * (straight.x + straight.y) + 2.0f * (float)PI * radius;
        float widths =
            borderStyle == UIBorderStyle::Dashed ? DASH_PERIOD_WIDTHS : DOT_PERIOD_WIDTHS;
        // Divided into a whole number of repeats here rather than in the shader, so
        // the pattern closes on itself instead of leaving a stub where the walk wraps.
        float count = Math::max(1.0f, Math::floor(perimeter / (borderWidth * widths) + 0.5f));
        dashPeriod = perimeter / count;
        dashRatio = DASH_ON_RATIO;
    }

    // The shadow lives outside the box, so the quad grows to hold it; the constant on
    // top is the box's own antialiased fringe, which would otherwise be cut off by
    // the very edge it is smoothing.
    Vec2 quadHalf = half + Vec2(shadowBlur) + shadowOffset.abs() + Vec2(AA_PADDING);

    // Corner order matches Renderer::addQuad so the shared 0,1,2 0,2,3 index buffer
    // winds correctly: bottom-left, bottom-right, top-right, top-left.
    const Vec2 corners[4] = {
        Vec2(-quadHalf.x, -quadHalf.y), Vec2(quadHalf.x, -quadHalf.y), Vec2(quadHalf.x, quadHalf.y),
        Vec2(-quadHalf.x, quadHalf.y)
    };

    BatchRenderer<UIBoxVertex>::Quad quad = m_batch.nextQuad(image);
    for (int i = 0; i < 4; i++) {
        quad.verts[i] = {
            drawPos + corners[i],
            corners[i],
            half,
            Vec4(0.0f, 0.0f, 1.0f, 1.0f),
            clipRect,
            fillColor,
            borderColor,
            shadowColor,
            shadowOffset,
            shadowBlur,
            radius,
            borderWidth,
            dashPeriod,
            dashRatio,
            0.0f,
            quad.texIndex
        };
    }
}

void UIRenderer::flush()
{
    if (!ensureReady()) return;
    m_batch.flush();
}

}   // namespace Engine
