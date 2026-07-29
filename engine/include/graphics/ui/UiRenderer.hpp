#pragma once

#include "graphics/Color.hpp"
#include "graphics/ui/layout/LayoutNode.hpp"
#include "graphics/ui/elements/UiElement.hpp"
#include "utils/Singleton.hpp"
#include <vector>

namespace Engine {

class GlTexture;

class UiRenderer : public Singleton<UiRenderer>
{
    friend class Singleton<UiRenderer>;

  private:
    GlTexture* m_whiteTexture = nullptr;
    Vec2 m_worldTopLeft = VEC2_ZERO;
    float m_worldPerUiUnit = 1.0f;
    float m_outlineThickness = 1.0f;
    float m_fillAlpha = 0.07f;
    bool m_drawBoxes = true;
    bool m_drawText = true;

  public:
    void setViewport(Vec2 worldTopLeft, float worldPerUiUnit);
    void setOutlineThickness(float thickness) { m_outlineThickness = thickness; }
    void setFillAlpha(float alpha) { m_fillAlpha = alpha; }
    void setDrawBoxes(bool value) { m_drawBoxes = value; }
    void setDrawText(bool value) { m_drawText = value; }
    bool getDrawBoxes() const { return m_drawBoxes; }

    void render(
        const std::vector<UiElement>& elements, uint elementCount,
        const std::vector<LayoutNode>& nodes
    );
    void release();

    Vec2 uiToWorld(Vec2 uiPos) const;
    float uiToWorldScale(float uiLength) const { return uiLength * m_worldPerUiUnit; }

  private:
    UiRenderer() = default;
    ~UiRenderer() = default;

    void ensureTexture();
    void renderBoxes(const std::vector<LayoutNode>& nodes);
    void renderText(
        const std::vector<UiElement>& elements, uint elementCount,
        const std::vector<LayoutNode>& nodes
    );
    void fillRect(Vec2 uiMin, Vec2 uiSize, Color color);
    void outlineRect(Vec2 uiMin, Vec2 uiSize, Color color);
    static Color depthColor(uint depth);
};

}   // namespace Engine
