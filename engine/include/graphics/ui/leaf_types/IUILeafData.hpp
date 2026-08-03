#pragma once

#include "utils/math/Vec2.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

enum class UILeafType
{
    None = 0,
    Text,
    Image,
};

struct UILeafWidths
{
    float min = 0.0f;
    float max = 0.0f;
};

struct IUILeafData
{
  private:
    UILeafType m_type = UILeafType::None;

  public:
    IUILeafData(UILeafType type) : m_type(type) {}
    virtual ~IUILeafData() = default;

    UILeafType getType() const { return m_type; }

    virtual UILeafWidths measureWidths() = 0;
    virtual float measureHeight(float contentWidth) = 0;
    virtual void draw(Vec2 drawPos, Vec2 size, Vec4 clipRect) = 0;
};

}   // namespace Engine
