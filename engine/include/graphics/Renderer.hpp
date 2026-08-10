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
    IdType m_defaultShaderId = INVALID_ID;
    BatchRenderer<VertexData> m_batch;

  public:
    void init(size_t maxQuadCount = 2000, GlShader* shader = nullptr);

    void setShader(GlShader* shader = nullptr);
    GlShader* getDefaultShader();
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
        Vec2 pos, Vec2 size, Color color, const GlTexture* texture, float angleRad = 0.0f,
        Vec2 uvMin = VEC2_ZERO, Vec2 uvMax = VEC2_ONE
    );

    void addLine(Vec2 start, Vec2 end, Color color, float thickness = 1.5f);
    void addFrame(Vec2 pos, Vec2 size, Color color, float thickness = 1.5f);
    void addCircleFrame(
        Vec2 pos, float radius, Color color, float thickness = 1.5f, float segmentDistance = 10.f
    );

  private:
    void syncRenderContext();
    void applyViewProj();
    void flush();

    Renderer() = default;
    ~Renderer() = default;
};

}   // namespace Engine
