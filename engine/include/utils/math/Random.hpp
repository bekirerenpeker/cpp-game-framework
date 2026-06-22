#pragma once

#include "utils/math/Vec2.hpp"
#include "utils/math/Vec3.hpp"

namespace Engine {

namespace Random {

void setSeed(unsigned int seed = 0);

int randomInt();
bool randomBool(float rate = 0.5f);
float float01();
float rangeFloat(float minInclusive, float maxInclusive);
int rangeInt(int minInclusive, int maxExclusive);

Vec2 pointInCircle(float radius);
Vec3 pointInSphere(float radius);

}   // namespace Random

}   // namespace Engine
