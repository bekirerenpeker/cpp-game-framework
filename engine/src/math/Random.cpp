#include "math/Random.hpp"
#include "math/Math.hpp"
#include <random>

namespace Random {

void setSeed(unsigned int seed) { srand(seed ? seed : (unsigned int)time(nullptr)); }

int randomInt() { return rand(); }

float float01() { return rand() / (float)RAND_MAX; }

bool randomBool(float rate) { return float01() < rate; }

float rangeFloat(float minInclusive, float maxInclusive)
{
    return float01() * (maxInclusive - minInclusive) + minInclusive;
}

int rangeInt(int minInclusive, int maxExclusive)
{
    return rand() % (maxExclusive - minInclusive) + minInclusive;
}

Vec2 pointInCircle(float radius)
{
    float angle = Random::rangeFloat(0, TAU);
    float dist = Random::rangeFloat(0, radius);
    return Vec2(Math::cos(angle), Math::sin(angle)) * dist;
}

// TODO: find another way to do this
Vec3 pointInSphere(float radius)
{
    Vec3 point(rangeFloat(-1, 1), rangeFloat(-1, 1), rangeFloat(-1, 1));
    float dist = Random::rangeFloat(0, radius);
    return point.normalized() * dist;
}

}   // namespace Random
