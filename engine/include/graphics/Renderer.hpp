#pragma once

#include "graphics/Color.hpp"
#include "graphics/gl_wrappers/GlBuffer.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
#include "graphics/gl_wrappers/GlVertexArray.hpp"
#include "utils/Singleton.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

struct VertexData
{
    Vec2 pos;
    Vec2 texCoords;
    Color color;
    int texIndex;
};

class Renderer : public Singleton<Renderer>
{
    friend class Singleton<Renderer>;

  private:
    const static int MAX_TEX_COUNT = 32;
    size_t m_maxQuadCount, m_maxVertexCount, m_maxIndexCount;

    GlBuffer m_vertexBuffer, m_indexBuffer;
    GlVertexArray m_vertexArray;
    GlShader* m_shader;

    GlTexture* m_textures[MAX_TEX_COUNT];
    int m_textureCount;

    VertexData* m_vertices = nullptr;
    size_t m_quadCount;

  public:
    void init(size_t maxQuadCount, GlShader* shader);

    void setShader(GlShader* shader);
    void setTarget();

    void clear();
    void clearColor(Color color);

    void beginScene();
    void endScene();

    void addQuad(Vec2 pos, Vec2 size, Color color, GlTexture* texture);

  private:
    void flush();
    int getTextureIndex(GlTexture* texture);

    Renderer();
    ~Renderer();
};

}   // namespace Engine
