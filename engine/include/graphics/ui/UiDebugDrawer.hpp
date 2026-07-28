#pragma once

#include "graphics/Color.hpp"
#include "graphics/ui/LayoutNode.hpp"
#include <vector>

namespace Engine {

class GlTexture;

class UiDebugDrawer
{
  private:
    GlTexture* m_whiteTexture = nullptr;
    Vec2 m_worldTopLeft = VEC2_ZERO;
    float m_worldUnitsPerUiUnit = 1.0f;
    float m_outlineThickness = 1.0f;
    float m_fillAlpha = 0.09f;
    bool m_drawFill = true;

  public:
    void setViewport(Vec2 worldTopLeft, float worldUnitsPerUiUnit);
    void setOutlineThickness(float thickness) { m_outlineThickness = thickness; }
    void setFillAlpha(float alpha) { m_fillAlpha = alpha; }
    void setDrawFill(bool value) { m_drawFill = value; }

    void draw(const std::vector<LayoutNode>& nodes);
    void release();

    Vec2 uiToWorld(Vec2 uiPos) const;

  private:
    void ensureTexture();
    void fillRect(Vec2 uiMin, Vec2 uiSize, Color color);
    void outlineRect(Vec2 uiMin, Vec2 uiSize, Color color);
    static Color depthColor(uint depth);
};

}   // namespace Engine
