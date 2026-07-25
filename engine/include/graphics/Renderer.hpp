#pragma once

#include "IRenderContext.hpp"
#include "core/window_management/ViewContext.hpp"
#include "ecs/registry/Registry.hpp"
#include "graphics/BatchRenderer.hpp"
#include "graphics/Color.hpp"
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
    IdType m_boundWindowId = INVALID_ID;
    BatchRenderer<VertexData> m_batch;

  public:
    void init(size_t maxQuadCount, GlShader* shader);

    void setShader(GlShader* shader);
    void releaseRenderContext();

    void beginPass();
    void drawToBuffer();
    void drawToWindow();

    void clear();
    void clearColor(Color color);

    void beginScene();
    void endScene();

    void renderSprites(Registry& registry);

    void addQuad(
        Vec2 pos, Vec2 size, Color color, const GlTexture* texture, Vec2 uvMin = VEC2_ZERO,
        Vec2 uvMax = VEC2_ONE, float angleRad = 0.0f
    );

  private:
    void syncRenderContext();
    void applyViewProj();
    void flush();

    Renderer() = default;
    ~Renderer() = default;
};

}   // namespace Engine
