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

Renderer::Renderer()
    : m_vertexBuffer(GlBufferType::VertexBuffer), m_indexBuffer(GlBufferType::IndexBuffer)
{
}

Renderer::~Renderer() { delete[] m_vertices; }

void Renderer::init(size_t maxQuadCount, GlShader* shader)
{
    m_maxQuadCount = maxQuadCount;
    m_maxVertexCount = maxQuadCount * 4;
    m_maxIndexCount = maxQuadCount * 6;
    m_shader = shader;

    unsigned int* indices = new unsigned int[m_maxIndexCount];
    size_t offset = 0;
    for (size_t i = 0; i < m_maxIndexCount; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;

        indices[i + 3] = offset + 0;
        indices[i + 4] = offset + 2;
        indices[i + 5] = offset + 3;

        offset += 4;
    }
    m_indexBuffer.setData(
        sizeof(unsigned int) * m_maxIndexCount, indices, GlBufferUsage::StaticDraw
    );
    delete[] indices;

    m_vertices = new VertexData[m_maxVertexCount];
    m_vertexBuffer.setData(
        m_maxVertexCount * sizeof(VertexData), nullptr, GlBufferUsage::DynamicDraw
    );

    m_quadCount = 0;
    m_textureCount = 0;
}

void Renderer::clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }
void Renderer::clearColor(Color color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    clear();
}

void Renderer::setShader(GlShader* shader)
{
    if (shader) m_shader = shader;
}
void Renderer::setViewProjMat(Registry& registry)
{
    m_viewProjMat = Mat4();

    Window* window = WindowManager::get().getWindow(m_renderWindowId);
    if (!window) return;
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

        m_viewProjMat = projMat * viewMat;
        break;
    }
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

    Mat4 viewProjMat = m_viewProjMat;
    m_viewProjMat.identity();

    addQuad(VEC2_ZERO, VEC2_ONE * 2, COLOR_WHITE, context->srcBuffer()->getTexture());
    endScene();

    m_viewProjMat = viewProjMat;
}
void Renderer::drawToWindow()
{
    if (m_renderWindowId == INVALID_ID) return;
    IRenderContext* context = WindowManager::get().getWindow(m_renderWindowId);

    Mat4 viewProjMat = m_viewProjMat;
    m_viewProjMat.identity();

    context->destBuffer()->unbind();
    Renderer::clearColor(Color(0, 0, 0, 0));
    addQuad(VEC2_ZERO, VEC2_ONE * 2, COLOR_WHITE, context->srcBuffer()->getTexture());
    endScene();

    m_viewProjMat = viewProjMat;
}

void Renderer::beginScene() {}
void Renderer::endScene() { flush(); }
void Renderer::flush()
{
    // if called before init or empty render
    if (m_renderWindowId == INVALID_ID || !m_shader || !m_vertices || m_quadCount == 0) {
        m_quadCount = 0;
        m_textureCount = 0;
        return;
    }

    IRenderContext* renderContext = WindowManager::get().getWindow(m_renderWindowId);
    if (!renderContext->m_vertexArray) {
        renderContext->m_vertexArray = new GlVertexArray();
        m_vertexBuffer.bind();
        renderContext->m_vertexArray->setLayout({
            {GlDataType::Float, 2},
            {GlDataType::Float, 2},
            {GlDataType::Float, 4},
            {  GlDataType::Int, 1},
        });
    }

    int texIndices[MAX_TEX_COUNT] = {0};
    for (int i = 0; i < m_textureCount; i++) {
        m_textures[i]->bind(i);
        texIndices[i] = i;
    }

    m_shader->bind();
    m_shader->setUniform<Mat4>("uMVP", m_viewProjMat);
    m_shader->setUniformArr("uTextures", m_textureCount, texIndices);

    renderContext->m_vertexArray->bind();
    m_indexBuffer.bind();
    m_vertexBuffer.setSubData(m_quadCount * 4 * sizeof(VertexData), m_vertices);
    glDrawElements(GL_TRIANGLES, m_quadCount * 6, GL_UNSIGNED_INT, 0);

    m_quadCount = 0;
    m_textureCount = 0;
}

int Renderer::getTextureIndex(GlTexture* texture)
{
    for (int i = 0; i < m_textureCount; i++) {
        if (m_textures[i] == texture) return i;
    }

    if (m_textureCount >= MAX_TEX_COUNT) flush();
    m_textures[m_textureCount] = texture;
    return m_textureCount++;
}

void Renderer::addQuad(Vec2 pos, Vec2 size, Color color, GlTexture* texture)
{
    if (m_quadCount >= m_maxQuadCount) flush();

    int texIndex = getTextureIndex(texture);
    VertexData* curr = m_vertices + m_quadCount * 4;

    curr[0] = {pos + Vec2(-1, -1) * size / 2.f, Vec2(0, 0), color, texIndex};
    curr[1] = {pos + Vec2(+1, -1) * size / 2.f, Vec2(1, 0), color, texIndex};
    curr[2] = {pos + Vec2(+1, +1) * size / 2.f, Vec2(1, 1), color, texIndex};
    curr[3] = {pos + Vec2(-1, +1) * size / 2.f, Vec2(0, 1), color, texIndex};

    m_quadCount++;
}
void Renderer::addQuad(Vec2 pos, Vec2 size, float angleRad, Color color, GlTexture* texture)
{
    // remove this angle == 0 check if it leads to performance issues
    if (angleRad == 0) return addQuad(pos, size, color, texture);
    if (m_quadCount >= m_maxQuadCount) flush();

    int texIndex = getTextureIndex(texture);
    VertexData* curr = m_vertices + m_quadCount * 4;

    curr[0] = {
        pos + Vec2(-1, -1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(0, 0), color,
        texIndex
    };
    curr[1] = {
        pos + Vec2(+1, -1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(1, 0), color,
        texIndex
    };
    curr[2] = {
        pos + Vec2(+1, +1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(1, 1), color,
        texIndex
    };
    curr[3] = {
        pos + Vec2(-1, +1).rotatedAround(VEC2_ZERO, angleRad) * size / 2.f, Vec2(0, 1), color,
        texIndex
    };

    m_quadCount++;
}

}   // namespace Engine
