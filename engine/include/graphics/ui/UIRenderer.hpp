#pragma once

#include "graphics/BatchRenderer.hpp"
#include "graphics/Color.hpp"
#include "graphics/ui/UIContainerStyle.hpp"
#include "utils/Singleton.hpp"
#include "utils/math/Mat4.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

// The rounded-box shader's format, not the UI's one vertex type -- text already runs
// on TextVertex, and anything bringing its own shader will bring its own too. One
// quad carries a whole box: the fragment shader rebuilds the background, the border
// ring and the shadow from a single distance field, so every style field is
// per-vertex data rather than a uniform -- a uniform would cost a draw call per style.
struct UIBoxVertex
{
    Vec2 pos;
    Vec2 localPos;
    Vec2 halfSize;
    Vec4 uvRect;   // uvMin.xy, uvMax.xy
    Color fillColor;
    Color borderColor;
    Color shadowColor;
    Vec2 shadowOffset;
    float shadowBlur;
    float cornerRadius;
    float borderWidth;
    float dashPeriod;
    float dashRatio;
    float reserved;
    int texIndex;
};

class UIRenderer : public Singleton<UIRenderer>
{
    friend class Singleton<UIRenderer>;

  private:
    static constexpr float AA_PADDING = 2.0f;
    static constexpr float DASH_PERIOD_WIDTHS = 4.0f;
    static constexpr float DOT_PERIOD_WIDTHS = 2.0f;
    static constexpr float DASH_ON_RATIO = 0.5f;

    BatchRenderer<UIBoxVertex> m_batch;
    Mat4 m_viewProjOverride;
    bool m_hasViewProjOverride = false;
    bool m_initialized = false;

  public:
    void init(GlShader* shader, size_t maxQuadCount = 2000);

    void setViewProjOverride(const Mat4& viewProj);
    void clearViewProjOverride();

    void addContainerQuad(Vec2 drawPos, Vec2 size, const UIContainerStyle& style);

    void flush();

  private:
    UIRenderer() = default;
    ~UIRenderer() = default;

    bool ensureReady();
};

}   // namespace Engine
