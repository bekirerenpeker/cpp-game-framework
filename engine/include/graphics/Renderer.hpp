#pragma once

#include "IRenderContext.hpp"
#include "ecs/registry/Registry.hpp"
#include "graphics/Color.hpp"
#include "graphics/gl_wrappers/GlBuffer.hpp"
#include "graphics/gl_wrappers/GlShader.hpp"
#include "graphics/gl_wrappers/GlTexture.hpp"
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

    IdType m_renderWindowId = INVALID_ID;
    Mat4 m_viewProjMat = Mat4();

    GlBuffer m_vertexBuffer, m_indexBuffer;
    GlShader* m_shader = nullptr;

    GlTexture* m_textures[MAX_TEX_COUNT];
    int m_textureCount;

    VertexData* m_vertices = nullptr;
    size_t m_quadCount;

  public:
    void init(size_t maxQuadCount, GlShader* shader);

    void setShader(GlShader* shader);
    void setViewProjMat(Registry& registry);

    void setRenderWindowId(IdType id);
    const IdType getRenderWindowId() const;

    const Mat4& getViewProjMat() const { return m_viewProjMat; }

    void beginPass();
    void drawToBuffer();
    void drawToWindow();

    void clear();
    void clearColor(Color color);

    void beginScene();
    void endScene();

    void addQuad(Vec2 pos, Vec2 size, Color color, GlTexture* texture);
    void addQuad(Vec2 pos, Vec2 size, float angleRad, Color color, GlTexture* texture);

  private:
    void flush();
    int getTextureIndex(GlTexture* texture);

    Renderer();
    ~Renderer();
};

}   // namespace Engine
