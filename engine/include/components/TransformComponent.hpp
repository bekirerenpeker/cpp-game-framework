#pragma once

#include "utils/math/Vec3.hpp"
#include "utils/math/Vec2.hpp"

namespace Engine {

struct TransformComponent
{
    Vec3 position = VEC3_ZERO;
    Vec2 scale = VEC2_ZERO;
    float rotation = 0.0f;

    TransformComponent() = default;
    TransformComponent(const TransformComponent&) = default;
    TransformComponent(const Vec3& pos) : position(pos) {}
};

}   // namespace Engine
