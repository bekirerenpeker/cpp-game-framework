#pragma once

#include "graphics/Color.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

class GlTexture;

struct SpriteComponent
{
    const GlTexture* texture = nullptr;
    Vec2 uvMin = VEC2_ZERO, uvMax = VEC2_ONE;
    Color color = COLOR_WHITE;
    int layer = 0;
    bool flipX = false, flipY = false;
    bool visible = true;

    SpriteComponent() = default;
    SpriteComponent(const GlTexture* tex) : texture(tex) {}
    SpriteComponent(const GlTexture* tex, Vec2 min, Vec2 max) : texture(tex), uvMin(min), uvMax(max)
    {
    }
};

}   // namespace Engine
