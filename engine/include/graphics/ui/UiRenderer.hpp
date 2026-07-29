#pragma once

#include "graphics/BatchRenderer.hpp"
#include "graphics/Color.hpp"
#include "graphics/ui/layout/LayoutNode.hpp"
#include "graphics/ui/elements/UiElement.hpp"
#include "utils/Singleton.hpp"
#include <vector>

namespace Engine {

// Positions are window pixels; localPos and halfSize are pixels from the rect centre,
// which is what lets the shader evaluate one rounded-box distance field per fragment.
struct UiVertex
{
    Vec2 pos;
    Vec2 localPos;
    Vec2 halfSize;
    Color fillColor;
    Color borderColor;
    float cornerRadius;
    float borderWidth;
};

class UiRenderer : public Singleton<UiRenderer>
{
    friend class Singleton<UiRenderer>;

  private:
    BatchRenderer<UiVertex> m_batch;
    Mat4 m_viewProjMat;
    Vec2 m_screenTopLeft = VEC2_ZERO;
    float m_pixelsPerUiUnit = 1.0f;
    float m_windowHeight = 0.0f;
    bool m_initialized = false;
    bool m_drawText = true;

  public:
    void init(GlShader* shader, size_t maxQuadCount = 2000);

    void setViewport(Vec2 screenTopLeft, float pixelsPerUiUnit);
    void setDrawText(bool value) { m_drawText = value; }

    Vec2 getScreenTopLeft() const { return m_screenTopLeft; }
    float getPixelsPerUiUnit() const { return m_pixelsPerUiUnit; }

    void render(
        const std::vector<UiElement>& elements, uint elementCount,
        const std::vector<LayoutNode>& nodes
    );
    void flush();

    Vec2 uiToScreen(Vec2 uiPos) const;
    float uiToScreenScale(float uiLength) const { return uiLength * m_pixelsPerUiUnit; }

  private:
    UiRenderer() = default;
    ~UiRenderer() = default;

    bool ensureReady();
    void renderRects(
        const std::vector<UiElement>& elements, uint elementCount,
        const std::vector<LayoutNode>& nodes
    );
    void renderText(
        const std::vector<UiElement>& elements, uint elementCount,
        const std::vector<LayoutNode>& nodes
    );
    void addRect(Vec2 uiMin, Vec2 uiSize, const UiStyle& style);
};

}   // namespace Engine
