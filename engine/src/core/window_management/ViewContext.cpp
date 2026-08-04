#include "core/window_management/ViewContext.hpp"
#include "components/CameraComponent.hpp"
#include "components/TransformComponent.hpp"
#include "core/input/Input.hpp"
#include "core/window_management/WindowManager.hpp"
#include "ecs/registry/View.hpp"
#include "utils/math/MathFuncs.hpp"

namespace Engine {

void ViewContext::setActiveWindow(IdType windowId)
{
    m_activeWindowId = windowId;

    Window* window = getActiveWindow();
    m_windowProjMat =
        window ? Mat4::ortho(
                     0.0f, (float)window->getWidth(), 0.0f, (float)window->getHeight(), -1.0f, 1.0f
                 ) :
                 Mat4();
}

Window* ViewContext::getActiveWindow() const
{
    return WindowManager::get().getWindow(m_activeWindowId);
}

void ViewContext::updateCamera(Registry& registry)
{
    resetCamera();

    Window* window = getActiveWindow();
    if (!window) return;
    float aspectRatio = window->getAspectRatio();

    View<TransformComponent, CameraComponent> view(registry);
    for (const auto& [entity, transform, cam] : view) {
        if (cam.windowId != m_activeWindowId || !cam.isPrimary) continue;

        float left = -cam.orthoSize * aspectRatio * 0.5f;
        float right = cam.orthoSize * aspectRatio * 0.5f;
        float bottom = -cam.orthoSize * 0.5f;
        float top = cam.orthoSize * 0.5f;

        m_viewMat = Mat4::view(transform.position, transform.rotation);
        m_projMat = Mat4::ortho(left, right, bottom, top, cam.nearClip, cam.farClip);
        m_viewProjMat = m_projMat * m_viewMat;

        m_activeCamera = entity;
        m_cameraPos = Vec2(transform.position.x, transform.position.y);
        m_cameraRotation = transform.rotation;
        m_worldUnitsPerPixel = window->getHeight() > 0 ? cam.orthoSize / window->getHeight() : 0.f;

        float halfW = right;
        float halfH = top;
        // A rotated camera still gets an axis-aligned bounds, so widen it to the
        // rotated rectangle's extents -- otherwise the corners cull as off-screen.
        if (transform.rotation != 0.0f) {
            float c = Math::abs(Math::cos(transform.rotation));
            float s = Math::abs(Math::sin(transform.rotation));
            halfW = right * c + top * s;
            halfH = right * s + top * c;
        }
        m_visibleBounds.min = m_cameraPos - Vec2(halfW, halfH);
        m_visibleBounds.max = m_cameraPos + Vec2(halfW, halfH);
        break;
    }
}

void ViewContext::resetCamera()
{
    m_activeCamera = NULL_ENTITY;
    m_viewMat = Mat4();
    m_projMat = Mat4();
    m_viewProjMat = Mat4();
    m_visibleBounds = WorldBounds {};
    m_cameraPos = VEC2_ZERO;
    m_cameraRotation = 0.0f;
    m_worldUnitsPerPixel = 0.0f;
}

bool ViewContext::isVisible(Vec2 pos, Vec2 halfExtents) const
{
    return pos.x + halfExtents.x >= m_visibleBounds.min.x &&
           pos.x - halfExtents.x <= m_visibleBounds.max.x &&
           pos.y + halfExtents.y >= m_visibleBounds.min.y &&
           pos.y - halfExtents.y <= m_visibleBounds.max.y;
}

Vec2 ViewContext::screenToWorld(Vec2 screenPos) const
{
    Vec2 offset = screenPos * m_worldUnitsPerPixel;
    if (m_cameraRotation != 0.0f) offset = offset.rotatedAround(VEC2_ZERO, m_cameraRotation);
    return m_cameraPos + offset;
}

Vec2 ViewContext::worldToScreen(Vec2 worldPos) const
{
    if (m_worldUnitsPerPixel == 0.0f) return VEC2_ZERO;

    Vec2 offset = worldPos - m_cameraPos;
    if (m_cameraRotation != 0.0f) offset = offset.rotatedAround(VEC2_ZERO, -m_cameraRotation);
    return offset / m_worldUnitsPerPixel;
}

Vec2 ViewContext::getMouseWorldPos() const { return screenToWorld(Input::get().getMousePos()); }

}   // namespace Engine
