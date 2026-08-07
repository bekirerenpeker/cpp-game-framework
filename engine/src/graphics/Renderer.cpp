#include "graphics/Renderer.hpp"
#include "components/TransformComponent.hpp"
#include "core/file_management/FileManager.hpp"
#include "core/resource_management/ResourceManager.hpp"
#include "components/SpriteComponent.hpp"
#include "core/window_management/ViewContext.hpp"
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
#include <functional>
#include <utility>

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
        shader ? shader : getDefaultShader()
    );
}

// Owned by the ResourceManager like any other heap resource, and loaded on first ask so
// a caller that supplies its own shader never pays for one it will not use.
GlShader* Renderer::getDefaultShader()
{
    if (m_defaultShaderId == INVALID_ID) {
        m_defaultShaderId = ResourceManager::get().addResource<GlShader>(
            FileManager::get().engineAsset("shaders/QuadShader.glsl")
        );
    }
    return ResourceManager::get().getResource<GlShader>(m_defaultShaderId);
}

void Renderer::clear()
{
    syncRenderContext();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
void Renderer::clearColor(Color color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    clear();
}

void Renderer::setShader(GlShader* shader) { m_batch.setShader(shader); }

void Renderer::syncRenderContext()
{
    IdType id = ViewContext::get().getActiveWindowId();
    IRenderContext* currContext = WindowManager::get().getWindow(m_boundWindowId);
    IRenderContext* newContext = WindowManager::get().getWindow(id);

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
    m_boundWindowId = id;
}
void Renderer::releaseRenderContext()
{
    IRenderContext* context = WindowManager::get().getWindow(m_boundWindowId);
    if (context) context->unbindRenderContext();
    m_boundWindowId = INVALID_ID;
}
void Renderer::applyViewProj() { m_batch.setViewProjMat(ViewContext::get().getViewProjMat()); }

void Renderer::beginPass()
{
    syncRenderContext();
    applyViewProj();
    if (m_boundWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_boundWindowId);

    endScene();
    context->switchBuffers();
    context->srcBuffer()->unbind();
    context->destBuffer()->bind();
    Renderer::clearColor(Color(0, 0, 0, 0));
    beginScene();
}
void Renderer::drawToBuffer()
{
    syncRenderContext();
    if (m_boundWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_boundWindowId);

    Mat4 viewProjMat = m_batch.getViewProjMat();
    m_batch.setViewProjMat(Mat4());

    addQuad(VEC2_ZERO, VEC2_ONE * 2, COLOR_WHITE, context->srcBuffer()->getTexture());
    endScene();

    m_batch.setViewProjMat(viewProjMat);
}
void Renderer::drawToWindow()
{
    syncRenderContext();
    if (m_boundWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_boundWindowId);

    Mat4 viewProjMat = m_batch.getViewProjMat();
    m_batch.setViewProjMat(Mat4());

    context->destBuffer()->unbind();
    Renderer::clearColor(Color(0, 0, 0, 0));
    addQuad(VEC2_ZERO, VEC2_ONE * 2, COLOR_WHITE, context->srcBuffer()->getTexture());
    endScene();

    m_batch.setViewProjMat(viewProjMat);
}

void Renderer::beginScene()
{
    syncRenderContext();
    applyViewProj();
}
void Renderer::endScene() { flush(); }
void Renderer::flush()
{
    syncRenderContext();

    // Guarded on isReady() (init() has run) so the lazy VAO creation below
    // never captures a not-yet-set vertex layout -- the first sync can land on
    // a window before init() runs.
    if (m_boundWindowId == INVALID_ID || !m_batch.isReady()) {
        m_batch.setVao(nullptr);
        m_batch.flush();
        return;
    }

    IRenderContext* renderContext = WindowManager::get().getWindow(m_boundWindowId);
    GlVertexArray*& vao = renderContext->vertexArray(this);
    if (!vao) {
        vao = new GlVertexArray();
        m_batch.configureVao(*vao);
    }
    m_batch.setVao(vao);

    m_batch.flush();
}

void Renderer::renderSprites(Registry& registry)
{
    registry.sort<SpriteComponent>([](const SpriteComponent& a, const SpriteComponent& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        if (a.shader != b.shader) return std::less<const GlShader*> {}(a.shader, b.shader);
        return std::less<const GlTexture*> {}(a.texture, b.texture);
    });

    GlShader* passShader = m_batch.getShader();
    GlShader* currShader = passShader;

    View<TransformComponent, SpriteComponent> view(registry);
    for (const auto& [entity, transform, sprite] : view.use<SpriteComponent>()) {
        if (!sprite.visible || !sprite.texture) continue;

        Vec2 pos(transform.position.x, transform.position.y);
        Vec2 size = transform.scale;

        Vec2 halfExtents = size.abs() / 2.f;
        if (transform.rotation != 0) halfExtents = Vec2(halfExtents.magnitude());

        if (!ViewContext::get().isVisible(pos, halfExtents)) continue;

        Vec2 uvMin = sprite.uvMin, uvMax = sprite.uvMax;
        if (sprite.flipX) std::swap(uvMin.x, uvMax.x);
        if (sprite.flipY) std::swap(uvMin.y, uvMax.y);

        // Switched after the culling checks so a skipped sprite never costs a flush.
        GlShader* shader = sprite.shader ? sprite.shader : passShader;
        if (shader != currShader) {
            flush();
            m_batch.setShader(shader);
            currShader = shader;
        }

        addQuad(pos, size, sprite.color, sprite.texture, uvMin, uvMax, transform.rotation);
    }

    if (currShader != passShader) {
        flush();
        m_batch.setShader(passShader);
    }
}

void Renderer::addQuad(
    Vec2 pos, Vec2 size, Color color, const GlTexture* texture, Vec2 uvMin, Vec2 uvMax,
    float angleRad
)
{
    const Vec2 corners[4] = {Vec2(-1, -1), Vec2(+1, -1), Vec2(+1, +1), Vec2(-1, +1)};
    const Vec2 uvs[4] = {
        {uvMin.x, uvMin.y},
        {uvMax.x, uvMin.y},
        {uvMax.x, uvMax.y},
        {uvMin.x, uvMax.y}
    };

    BatchRenderer<VertexData>::Quad quad = m_batch.nextQuad(texture);

    // remove this angle == 0 check if it leads to performance issues
    bool rotated = angleRad != 0;
    for (int i = 0; i < 4; i++) {
        Vec2 corner = rotated ? corners[i].rotatedAround(VEC2_ZERO, angleRad) : corners[i];
        quad.verts[i] = {pos + corner * size / 2.f, uvs[i], color, quad.texIndex};
    }
}

void Renderer::addLine(Vec2 start, Vec2 end, Color color, float thickness)
{
    if (m_boundWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_boundWindowId);
    if (!context || !context->srcBuffer()) return;

    Vec2 direction = end - start;
    float length = direction.magnitude();
    if (length == 0) return;

    Vec2 center = (start + end) * 0.5f;
    float angle = std::atan2(direction.y, direction.x);

    addQuad(
        center, Vec2(length, thickness), color, context->srcBuffer()->getTexture(), VEC2_ZERO,
        VEC2_ONE, angle
    );
}

void Renderer::addFrame(Vec2 pos, Vec2 size, Color color, float thickness)
{
    if (m_boundWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_boundWindowId);
    if (!context || !context->srcBuffer()) return;

    GlTexture* texture = context->srcBuffer()->getTexture();

    // Top edge (full width)
    addQuad(
        pos + Vec2(size.x * 0.5f, thickness * 0.5f), Vec2(size.x, thickness), color, texture,
        VEC2_ZERO, VEC2_ONE
    );

    // Bottom edge (full width)
    addQuad(
        pos + Vec2(size.x * 0.5f, size.y - thickness * 0.5f), Vec2(size.x, thickness), color,
        texture, VEC2_ZERO, VEC2_ONE
    );

    // Left edge (excluding corners)
    addQuad(
        pos + Vec2(thickness * 0.5f, size.y * 0.5f), Vec2(thickness, size.y - 2 * thickness), color,
        texture, VEC2_ZERO, VEC2_ONE
    );

    // Right edge (excluding corners)
    addQuad(
        pos + Vec2(size.x - thickness * 0.5f, size.y * 0.5f),
        Vec2(thickness, size.y - 2 * thickness), color, texture, VEC2_ZERO, VEC2_ONE
    );
}

}   // namespace Engine
