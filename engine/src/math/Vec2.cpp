#include "math/Vec2.hpp"
#include "math/Math.hpp"
#include "math/Vec3.hpp"
#include "math/Vec4.hpp"

Vec2::Vec2(float x, float y) : x(x), y(y) {}
Vec2::Vec2(float s) : x(s), y(s) {}
Vec2::Vec2() : x(0), y(0) {}

// conversion operators
Vec2::operator Vec3() const { return Vec3(x, y, 0); }
Vec2::operator Vec4() const { return Vec4(x, y, 0, 1); }

// printing operator
std::ostream& operator<<(std::ostream& stream, const Vec2& v)
{
    stream << "Vec2(" << v.x << ", " << v.y << ")";
    return stream;
}

// arithmetic operators
Vec2 Vec2::operator-() const { return Vec2(-x, -y); }

Vec2 Vec2::operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
Vec2 Vec2::operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
Vec2 Vec2::operator*(const Vec2& other) const { return Vec2(x * other.x, y * other.y); }
Vec2 Vec2::operator/(const Vec2& other) const { return Vec2(x / other.x, y / other.y); }

Vec2 Vec2::operator*(float other) const { return Vec2(x * other, y * other); }
Vec2 Vec2::operator/(float other) const { return Vec2(x / other, y / other); }
Vec2 operator*(float left, const Vec2& right) { return Vec2(left * right.x, left * right.y); }

Vec2& Vec2::operator+=(const Vec2& other)
{
    *this = *this + other;
    return *this;
}
Vec2& Vec2::operator-=(const Vec2& other)
{
    *this = *this - other;
    return *this;
}
Vec2& Vec2::operator*=(const Vec2& other)
{
    *this = *this * other;
    return *this;
}
Vec2& Vec2::operator/=(const Vec2& other)
{
    *this = *this / other;
    return *this;
}

Vec2& Vec2::operator*=(float other)
{
    *this = *this * other;
    return *this;
}
Vec2& Vec2::operator/=(float other)
{
    *this = *this / other;
    return *this;
}

// compariosn operators
bool Vec2::operator==(const Vec2& other) const { return x == other.x && y == other.y; }
bool Vec2::operator!=(const Vec2& other) const { return !(*this == other); }

// member functions
float Vec2::magnitude() const { return Math::sqrt(x * x + y * y); }
float Vec2::sqrMagnitude() const { return x * x + y * y; }
Vec2 Vec2::normalized() const { return *this / magnitude(); }
void Vec2::normalize() { *this /= magnitude(); }

Vec2 Vec2::abs() const { return Vec2(Math::abs(x), Math::abs(y)); }
Vec2 Vec2::round() const { return Vec2(Math::round(x), Math::round(y)); }
Vec2 Vec2::floor() const { return Vec2(Math::floor(x), Math::floor(y)); }
Vec2 Vec2::ceil() const { return Vec2(Math::ceil(x), Math::ceil(y)); }
float Vec2::angle() const { return Math::atan2(y, x); }

Vec2 Vec2::magnitudeClamped(float length) const
{
    if (sqrMagnitude() <= length * length) return *this;
    return normalized() * length;
}
void Vec2::clampMagnitude(float length)
{
    if (sqrMagnitude() > length * length) *this = normalized() * length;
}

void Vec2::rotateAround(Vec2 pivot, float angle)
{
    *this = Vec2::rotateAround(*this, pivot, angle);
}

// static functions
float Vec2::sqrDist(const Vec2& a, const Vec2& b) { return (a - b).sqrMagnitude(); }
float Vec2::dist(const Vec2& a, const Vec2& b) { return (a - b).magnitude(); }
Vec2 Vec2::dir(const Vec2& from, const Vec2& to) { return (to - from).normalized(); }
float Vec2::angle(const Vec2& from, const Vec2& to) { return (to - from).angle(); }

Vec2 Vec2::min(const Vec2& a, const Vec2& b)
{
    return Vec2(Math::min(a.x, b.x), Math::min(a.y, b.y));
}
Vec2 Vec2::max(const Vec2& a, const Vec2& b)
{
    return Vec2(Math::max(a.x, b.x), Math::max(a.y, b.y));
}

float Vec2::dot(const Vec2& a, const Vec2& b) { return a.x * b.x + a.y * b.y; }
float Vec2::dotN(const Vec2& a, const Vec2& b) { return dot(a.normalized(), b.normalized()); }

Vec2 Vec2::rotateAround(const Vec2& v, const Vec2& pivot, float angle)
{
    float newX = (v.x - pivot.x) * Math::cos(angle) - (v.y - pivot.y) * Math::sin(angle);
    float newY = (v.y - pivot.y) * Math::cos(angle) + (v.x - pivot.x) * Math::sin(angle);
    return Vec2(newX, newY) + pivot;
}

Vec2 Vec2::perpendicular(const Vec2& v) { return Vec2(v.y, -v.x); }
Vec2 Vec2::reflect(const Vec2& ray, const Vec2& normal)
{
    return ray - 2 * dot(ray, normal) * normal;
}

Vec2 Vec2::lerp(const Vec2& from, const Vec2& to, float t) { return (to - from) * t + from; }
float Vec2::inverseLerp(const Vec2& from, const Vec2& to, const Vec2& val)
{
    if (Math::abs(from.x - to.x) > Math::abs(from.y - to.y))
        return Math::inverseLerp(from.x, to.x, val.x);
    else return Math::inverseLerp(from.y, to.y, val.y);
}
Vec2 Vec2::moveTowards(const Vec2& from, const Vec2& to, float step)
{
    if (sqrDist(from, to) > step * step) return from + dir(from, to) * step;
    else return to;
}
