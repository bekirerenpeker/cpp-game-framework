#pragma once

#include "ecs/registry/Entity.hpp"
#include "utils/Singleton.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Mat4.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

class Window;
class Registry;

// Axis-aligned world-space rectangle, e.g. the camera's visible area.
struct WorldBounds
{
    Vec2 min = VEC2_ZERO;
    Vec2 max = VEC2_ZERO;
};

class ViewContext : public Singleton<ViewContext>
{
    friend class Singleton<ViewContext>;

  private:
    IdType m_activeWindowId = INVALID_ID;
    Entity m_activeCamera = NULL_ENTITY;

    Mat4 m_viewMat;
    Mat4 m_projMat;
    Mat4 m_viewProjMat;
    Mat4 m_windowProjMat;
    WorldBounds m_visibleBounds;

    Vec2 m_cameraPos = VEC2_ZERO;
    float m_cameraRotation = 0.0f;
    float m_worldUnitsPerPixel = 0.0f;

  public:
    void setActiveWindow(IdType windowId);
    IdType getActiveWindowId() const { return m_activeWindowId; }
    Window* getActiveWindow() const;

    void updateCamera(Registry& registry);
    Entity getActiveCamera() const { return m_activeCamera; }

    const Mat4& getViewMat() const { return m_viewMat; }
    const Mat4& getProjMat() const { return m_projMat; }
    const Mat4& getViewProjMat() const { return m_viewProjMat; }
    const Mat4& getWindowProjMat() const { return m_windowProjMat; }
    const WorldBounds& getVisibleWorldBounds() const { return m_visibleBounds; }
    bool isVisible(Vec2 pos, Vec2 halfExtents) const;

    Vec2 screenToWorld(Vec2 screenPos) const;   // screenPos: window-centered px, +Y up
    Vec2 worldToScreen(Vec2 worldPos) const;
    Vec2 getMouseWorldPos() const;

  private:
    void resetCamera();

    ViewContext() = default;
    ~ViewContext() = default;
};

}   // namespace Engine
