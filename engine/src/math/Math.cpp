#include "math/Math.hpp"
#include <cmath>

namespace Math {

float abs(float x) { return x > 0 ? x : -x; }
float sign(float x) { return x == 0 ? 0 : (x > 0 ? 1.f : -1.f); }
float exp(float x) { return std::exp(x); }
float pow(float base, float pow) { return std::pow(base, pow); }
float sqrt(float x) { return std::sqrt(x); }
float mod(float x, float y) { return std::fmod(x, y); }

float ln(float x) { return std::log(x); }
float log10(float x) { return std::log10(x); }
float log2(float x) { return std::log2(x); }

float trunc(float x) { return std::trunc(x); }
float ceil(float x) { return std::ceil(x); }
float floor(float x) { return std::floor(x); }
float round(float x, int decimal)
{
    float scalar = pow(10.f, decimal - 1.f);
    return std::round(x * scalar) / scalar;
}

float min(float x, float y) { return x > y ? y : x; }
float max(float x, float y) { return x < y ? y : x; }
float clamp(float x, float minVal, float maxVal) { return min(maxVal, max(x, minVal)); }

float lerp(float a, float b, float t) { return (b - a) * t + a; }
float inverseLerp(float a, float b, float val) { return (val - a) / (b - a); }
float moveTowards(float a, float b, float step)
{
    if (abs(a - b) > step) return a + sign(b - a) * step;
    else return b;
}

float pingPong(float x, float start, float end)
{
    float rem = mod(x - start, end - start);
    if (static_cast<int>((x - start) / (end - start)) % 2 == 0) return start + rem;
    else return end - rem;
}

float deltaAngle(float a, float b)
{
    bool warp = Math::abs(b - a) < TAU - Math::abs(b - a);
    if (warp) return (TAU - Math::abs(b - a)) * Math::sign(a - b);
    else return Math::abs(b - a) * Math::sign(b - a);
}
float lerpAngle(float a, float b, float t)
{
    bool warp = Math::abs(b - a) < TAU - Math::abs(b - a);
    if (warp) return a + (TAU - Math::abs(b - a)) * t * -Math::sign(b - a);
    else return Math::lerp(a, b, t);
}
float moveTowardsAngle(float a, float b, float step)
{
    bool warp = Math::abs(b - a) < TAU - Math::abs(b - a);
    float len = warp ? TAU - Math::abs(b - a) : Math::abs(b - a);
    if (len < step) return b;
    int dir = (warp ? -1 : 1) * Math::sign(b - a);
    return a + step * dir;
}

float perlin2D(float x, float y, int seed)
{
    // return stb_perlin_noise3_seed(x, y, 0, 0, 0, 0, seed);
    return 0;
}
float perlin3D(float x, float y, float z, int seed)
{
    // return stb_perlin_noise3_seed(x, y, z, 0, 0, 0, seed);
    return 0;
}

float sin(float radians) { return std::sin(radians); }
float cos(float radians) { return std::cos(radians); }
float tan(float radians) { return std::tan(radians); }

float asin(float radians) { return std::asin(radians); }
float acos(float radians) { return std::acos(radians); }
float atan(float radians) { return std::atan(radians); }
float atan2(float y, float x) { return std::atan2(y, x); }

}   // namespace Math
