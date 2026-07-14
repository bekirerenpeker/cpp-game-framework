#include "graphics/Renderer.hpp"
#include "components/TransformComponent.hpp"
#include "components/CameraComponent.hpp"
#include "core/logging/LoggerMacros.hpp"
#include "core/window_management/WindowManager.hpp"
#include "ecs/registry/View.hpp"
#include "graphics/Color.hpp"
#include "graphics/IRenderContext.hpp"
#include "graphics/gl_wrappers/GlFrameBuffer.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "glad/glad.h"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "utils/TypeAliases.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

void Renderer::init(size_t maxQuadCount, GlShader* shader)
{
    m_batch.init(
        maxQuadCount,
        {
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {  GlDataType::Int, 1},
    },
        shader
    );
}

void Renderer::clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }
void Renderer::clearColor(Color color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    clear();
}

void Renderer::setShader(GlShader* shader) { m_batch.setShader(shader); }
void Renderer::setViewProjMat(Registry& registry)
{
    Mat4 viewProjMat;

    Window* window = WindowManager::get().getWindow(m_renderWindowId);
    if (!window) {
        m_batch.setViewProjMat(viewProjMat);
        return;
    }
    float aspectRatio = window->getAspectRatio();

    View<TransformComponent, CameraComponent> view(registry);
    for (const auto& [entity, transform, cam] : view) {
        if (cam.windowId != m_renderWindowId || !cam.isPrimary) continue;

        float left = -cam.orthoSize * aspectRatio * 0.5f;
        float right = cam.orthoSize * aspectRatio * 0.5f;
        float bottom = -cam.orthoSize * 0.5f;
        float top = cam.orthoSize * 0.5f;

        Mat4 viewMat = Mat4::view(transform.position, transform.rotation);
        Mat4 projMat = Mat4::ortho(left, right, bottom, top, cam.nearClip, cam.farClip);

        viewProjMat = projMat * viewMat;
        break;
    }

    m_batch.setViewProjMat(viewProjMat);
}
void Renderer::setRenderWindowId(IdType id)
{
    IRenderContext* currContext = WindowManager::get().getWindow(m_renderWindowId);
    IRenderContext* newContext = WindowManager::get().getWindow(id);

    if (id == INVALID_ID) {
        if (currContext) currContext->unbindRenderContext();
        m_renderWindowId = INVALID_ID;
        return;
    }

    if (!newContext || (newContext == currContext && !currContext->m_isRenderContextDirty)) return;
    if (currContext) currContext->unbindRenderContext();
    newContext->bindRenderContext();

    if (newContext->m_contextBuffers[0] == nullptr) {
        newContext->m_contextBuffers[0] = new GlFrameBuffer(
            newContext->getRenderContextWidth(), newContext->getRenderContextHeight()
        );
        newContext->m_contextBuffers[1] = new GlFrameBuffer(
            newContext->getRenderContextWidth(), newContext->getRenderContextHeight()
        );
    }

    glViewport(0, 0, newContext->getRenderContextWidth(), newContext->getRenderContextHeight());
    newContext->m_isRenderContextDirty = false;
    m_renderWindowId = id;
}
const IdType Renderer::getRenderWindowId() const { return m_renderWindowId; }

void Renderer::beginPass()
{
    if (m_renderWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_renderWindowId);

    endScene();
    context->switchBuffers();
    context->srcBuffer()->unbind();
    context->destBuffer()->bind();
    Renderer::clearColor(Color(0, 0, 0, 0));
    beginScene();
}
void Renderer::drawToBuffer()
{
    if (m_renderWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_renderWindowId);

    Mat4 viewProjMat = m_batch.getViewProjMat();
    m_batch.setViewProjMat(Mat4());

    addQuad(VEC2_ZERO, VEC2_ONE * 2, COLOR_WHITE, context->srcBuffer()->getTexture());
    endScene();

    m_batch.setViewProjMat(viewProjMat);
}
void Renderer::drawToWindow()
{
    if (m_renderWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_renderWindowId);

    Mat4 viewProjMat = m_batch.getViewProjMat();
    m_batch.setViewProjMat(Mat4());

    context->destBuffer()->unbind();
    Renderer::clearColor(Color(0, 0, 0, 0));
    addQuad(VEC2_ZERO, VEC2_ONE * 2, COLOR_WHITE, context->srcBuffer()->getTexture());
    endScene();

    m_batch.setViewProjMat(viewProjMat);
}

void Renderer::beginScene() {}
void Renderer::endScene() { flush(); }
void Renderer::flush()
{
    // Guarded on isReady() (init() has run) so the lazy VAO creation below
    // never captures a not-yet-set vertex layout -- WindowManager triggers a
    // setRenderWindowId() for the very first window before init() runs.
    if (m_renderWindowId == INVALID_ID || !m_batch.isReady()) {
        m_batch.setVao(nullptr);
        m_batch.flush();
        return;
    }

    IRenderContext* renderContext = WindowManager::get().getWindow(m_renderWindowId);
    if (!renderContext->m_vertexArray) {
        renderContext->m_vertexArray = new GlVertexArray();
        m_batch.configureVao(*renderContext->m_vertexArray);
    }
    m_batch.setVao(renderContext->m_vertexArray);

    m_batch.flush();
}

void Renderer::addQuad(Vec2 pos, Vec2 size, Color color, GlTexture* texture)
{
    BatchRenderer<VertexData>::Quad quad = m_batch.nextQuad(texture);

    quad.verts[0] = {pos + Vec2(-1, -1) * size / 2.f, Vec2(0, 0), color, quad.texIndex};
    quad.verts[1] = {pos + Vec2(+1, -1) * size / 2.f, Vec2(1, 0), color, quad.texIndex};
    quad.verts[2] = {pos + Vec2(+1, +1) * size / 2.f, Vec2(1, 1), color, quad.texIndex};
    quad.verts[3] = {pos + Vec2(-1, +1) * size / 2.f, Vec2(0, 1), color, quad.texIndex};
}
void Renderer::addQuad(Vec2 pos, Vec2 size, float angleRad, Color color, GlTexture* texture)
{
    // remove this angle == 0 check if it leads to performance issues
    if (angleRad == 0) return addQuad(pos, size, color, texture);

    BatchRenderer<VertexData>::Quad quad = m_batch.nextQuad(texture);

    quad.verts[0] = {
        pos + Vec2(-1, -1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(0, 0), color,
        quad.texIndex
    };
    quad.verts[1] = {
        pos + Vec2(+1, -1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(1, 0), color,
        quad.texIndex
    };
    quad.verts[2] = {
        pos + Vec2(+1, +1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(1, 1), color,
        quad.texIndex
    };
    quad.verts[3] = {
        pos + Vec2(-1, +1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(0, 1), color,
        quad.texIndex
    };
}

}   // namespace Engine
