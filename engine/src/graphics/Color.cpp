#include "graphics/Color.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

Color::Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

Color::Color(float c, float a) : r(c), g(c), b(c), a(a) {}

Color::Color() : r(0), g(0), b(0), a(0) {}

Color::operator Vec4() const { return Vec4(r, g, b, a); }

}   // namespace Engine
