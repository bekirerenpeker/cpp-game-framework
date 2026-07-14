#pragma once

#include "graphics/gl_wrappers/GlBuffer.hpp"
#include "graphics/gl_wrappers/GlLayout.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "graphics/gl_wrappers/GlVertexArray.hpp"
#include "utils/math/Mat4.hpp"
#include <glad/glad.h>
#include <vector>

namespace Engine {

// A quad batch renderer generic over the vertex type. It owns a dynamic vertex
// buffer, a shared static index buffer (0,1,2, 0,2,3), its own VAO, and a
// texture-slot table. The caller supplies the vertex layout at init and fills
// quads through nextQuad(). Header-only because it is a template.
template<typename Vertex> class BatchRenderer
{
  private:
    static constexpr int MAX_TEX_COUNT = 32;
    size_t m_maxQuadCount = 0, m_quadCount = 0;
    GlBuffer m_vertexBuffer, m_indexBuffer;
    GlVertexArray m_vao;
    GlShader* m_shader = nullptr;
    Mat4 m_viewProjMat;
    Vertex* m_vertices = nullptr;
    const GlTexture* m_textures[MAX_TEX_COUNT];
    int m_textureCount = 0;

  public:
    struct Quad
    {
        Vertex* verts;
        int texIndex;
    };

    BatchRenderer()
        : m_vertexBuffer(GlBufferType::VertexBuffer), m_indexBuffer(GlBufferType::IndexBuffer)
    {
    }
    ~BatchRenderer() { delete[] m_vertices; }

    void init(size_t maxQuadCount, const GlLayout& layout, GlShader* shader)
    {
        m_maxQuadCount = maxQuadCount;
        m_shader = shader;

        size_t maxIndexCount = maxQuadCount * 6;
        std::vector<unsigned int> indices(maxIndexCount);
        unsigned int offset = 0;
        for (size_t i = 0; i < maxIndexCount; i += 6) {
            indices[i + 0] = offset + 0;
            indices[i + 1] = offset + 1;
            indices[i + 2] = offset + 2;
            indices[i + 3] = offset + 0;
            indices[i + 4] = offset + 2;
            indices[i + 5] = offset + 3;
            offset += 4;
        }

        m_vao.bind();
        m_vertexBuffer.bind();
        m_vertexBuffer.setData(
            static_cast<long>(maxQuadCount * 4 * sizeof(Vertex)), nullptr,
            GlBufferUsage::DynamicDraw
        );
        m_vao.setLayout(layout);
        m_indexBuffer.bind();
        m_indexBuffer.setData(
            static_cast<long>(maxIndexCount * sizeof(unsigned int)), indices.data(),
            GlBufferUsage::StaticDraw
        );

        delete[] m_vertices;
        m_vertices = new Vertex[maxQuadCount * 4];
        m_quadCount = 0;
        m_textureCount = 0;
    }

    void setShader(GlShader* shader)
    {
        if (shader) m_shader = shader;
    }
    void setViewProjMat(const Mat4& viewProj) { m_viewProjMat = viewProj; }

    // Reserves one quad's 4 vertices and resolves the texture to a slot. The
    // full/flush checks happen before the slot is resolved so the returned
    // texIndex stays valid against the buffer that will actually be drawn.
    Quad nextQuad(const GlTexture* texture)
    {
        if (m_quadCount >= m_maxQuadCount) flush();
        int texIndex = getTextureIndex(texture);
        Quad quad {m_vertices + m_quadCount * 4, texIndex};
        m_quadCount++;
        return quad;
    }

    void flush()
    {
        if (!m_shader || !m_vertices || m_quadCount == 0) {
            m_quadCount = 0;
            m_textureCount = 0;
            return;
        }

        int texSlots[MAX_TEX_COUNT] = {0};
        for (int i = 0; i < m_textureCount; i++) {
            m_textures[i]->bind(i);
            texSlots[i] = i;
        }

        m_shader->bind();
        m_shader->setUniform<Mat4>("uMVP", m_viewProjMat);
        m_shader->setUniformArr("uTextures", m_textureCount, texSlots);

        m_vao.bind();
        m_indexBuffer.bind();
        m_vertexBuffer.setSubData(static_cast<long>(m_quadCount * 4 * sizeof(Vertex)), m_vertices);
        glDrawElements(GL_TRIANGLES, static_cast<int>(m_quadCount * 6), GL_UNSIGNED_INT, 0);

        m_quadCount = 0;
        m_textureCount = 0;
    }

  private:
    int getTextureIndex(const GlTexture* texture)
    {
        for (int i = 0; i < m_textureCount; i++) {
            if (m_textures[i] == texture) return i;
        }

        if (m_textureCount >= MAX_TEX_COUNT) flush();
        m_textures[m_textureCount] = texture;
        return m_textureCount++;
    }
};

}   // namespace Engine
