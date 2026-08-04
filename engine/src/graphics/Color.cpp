#include "graphics/Color.hpp"
#include "utils/math/MathFuncs.hpp"
#include "utils/math/Vec3.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

Color::Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

Color::Color(float c, float a) : r(c), g(c), b(c), a(a) {}

Color::Color() : r(0), g(0), b(0), a(0) {}

Color::operator Vec4() const { return Vec4(r, g, b, a); }

// Hue is undefined for a greyscale colour and saturation for black, so both come back
// as zero there rather than as something arbitrary -- a caller that needs them to
// survive a round trip through such a value has to keep its own copy.
Vec3 Color::toHsv() const
{
    float maxC = Math::max(r, Math::max(g, b));
    float minC = Math::min(r, Math::min(g, b));
    float delta = maxC - minC;

    float hue = 0.0f;
    if (delta > 0.0001f) {
        if (maxC == r) hue = (g - b) / delta;
        else if (maxC == g) hue = 2.0f + (b - r) / delta;
        else hue = 4.0f + (r - g) / delta;

        hue /= 6.0f;
        if (hue < 0.0f) hue += 1.0f;
    }
    return Vec3(hue, maxC > 0.0f ? delta / maxC : 0.0f, maxC);
}

Color Color::fromHsv(const Vec3& hsv, float a)
{
    // Short of 1 so a full-circle hue lands back on the first sector rather than a sixth
    // one that does not exist.
    float h = Math::clamp(hsv.x, 0.0f, 0.9999f) * 6.0f;
    float s = Math::clamp(hsv.y, 0.0f, 1.0f);
    float v = Math::clamp(hsv.z, 0.0f, 1.0f);

    int sector = (int)Math::floor(h);
    float f = h - (float)sector;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    switch (sector) {
    case 0 : return Color(v, t, p, a);
    case 1 : return Color(q, v, p, a);
    case 2 : return Color(p, v, t, a);
    case 3 : return Color(p, q, v, a);
    case 4 : return Color(t, p, v, a);
    default: return Color(v, p, q, a);
    }
}

}   // namespace Engine
