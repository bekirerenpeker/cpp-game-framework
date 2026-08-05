#pragma once

#include "IUILeafData.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

class GlShader;

// params0/params1 ride in the box vertex's fill and border colour slots, so a shader
// leaf needs no vertex format or batch of its own. intrinsicSize is the leaf's
// content size; a Grow or Fixed spec on the node overrides it.
struct UIShaderConfig
{
    GlShader* shader = nullptr;
    Vec4 params0;
    Vec4 params1;
    Vec2 intrinsicSize = Vec2(64.0f, 64.0f);
};

class UIShaderLeafData : public IUILeafData
{
  private:
    UIShaderConfig m_config;

  public:
    UIShaderLeafData(const UIShaderConfig& config);
    ~UIShaderLeafData() = default;

    UILeafWidths measureWidths() override;
    float measureHeight(float contentWidth) override;
    void draw(Vec2 drawPos, Vec2 size, Vec4 clipRect) override;
};

}   // namespace Engine
