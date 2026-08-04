#pragma once

#include "graphics/BatchRenderer.hpp"
#include "graphics/Color.hpp"
#include "graphics/ui/styling/UIContainerStyle.hpp"
#include "utils/Singleton.hpp"
#include "utils/math/Mat4.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

// Which space a UI unit lands in. Screen is one unit per window pixel, unaffected by
// the camera -- what a HUD or menu wants. World moves and scales with the camera like
// any sprite, for a nameplate or a diegetic panel. The whole UI shares one space.
enum class UISpace : uint8_t
{
    Screen = 0,
    World,
};

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
    Vec4 uvRect;     // uvMin.xy, uvMax.xy
    Vec4 clipRect;   // minX, minY, maxX, maxY, in the same space as pos
    Color fillColor;
    Color borderColor;
    Color shadowColor;
    Vec2 shadowOffset;
    float shadowBlur;
    float shadowReserved;   // pads the attribute out to a full vec4 slot, unused in the shader
    Vec4 cornerRadii;       // topLeft, topRight, bottomRight, bottomLeft
    float borderWidth;
    float dashPeriod;
    float dashRatio;
    float borderReserved;
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
    UISpace m_space = UISpace::Screen;
    bool m_initialized = false;
    IdType m_defaultShaderId = INVALID_ID;

  public:
    // A null shader takes the engine's own, which is what almost every caller wants;
    // pass one only to render the box batch through something else.
    void init(GlShader* shader = nullptr, size_t maxQuadCount = 2000);
    void setShader(GlShader* shader);

    void setSpace(UISpace space);
    UISpace getSpace() const { return m_space; }

    Mat4 getViewProjMat() const;
    Vec2 getRootOrigin() const;
    Vec2 getMouseUiPos() const;

    void addContainerQuad(Vec2 drawPos, Vec2 size, const UIContainerStyle& style, Vec4 clipRect);
    void addShaderQuad(
        GlShader* shader, Vec2 drawPos, Vec2 size, Vec4 clipRect, Vec4 params0, Vec4 params1
    );

    void flush();

  private:
    UIRenderer() = default;
    ~UIRenderer() = default;

    bool ensureReady();
};

}   // namespace Engine
