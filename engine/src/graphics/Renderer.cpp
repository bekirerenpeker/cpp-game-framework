#include "graphics/Renderer.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "glad/glad.h"
#include "graphics/gl_wrappers/GlTexture.hpp"

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

    m_vertexArray.bind();
    m_vertexBuffer.setData(
        m_maxVertexCount * sizeof(VertexData), nullptr, GlBufferUsage::DynamicDraw
    );
    m_vertexArray.setLayout({
        {GlDataType::Float, 2},
        {GlDataType::Float, 2},
        {GlDataType::Float, 4},
        {  GlDataType::Int, 1},
    });

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
void Renderer::setTarget()
{
    // set target to either a window or a frame buffer
}

void Renderer::beginScene() { m_quadCount = 0; }
void Renderer::endScene() { flush(); }
void Renderer::flush()
{
    int texIndices[MAX_TEX_COUNT] = {0};
    for (int i = 0; i < m_textureCount; i++) {
        m_textures[i]->bind(i);
        texIndices[i] = i;
    }

    m_shader->bind();
    m_shader->setUniform<Mat4>("uMVP", Mat4());
    m_shader->setUniformArr("uTextures", m_textureCount, texIndices);

    m_vertexArray.bind();
    m_indexBuffer.bind();
    m_vertexBuffer.setData(
        m_quadCount * 4 * sizeof(VertexData), m_vertices, GlBufferUsage::DynamicDraw
    );
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

}   // namespace Engine
