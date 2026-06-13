#include "utils/math/Vec3.hpp"
#include "utils/math/Vec2.hpp"
#include "utils/math/Vec4.hpp"
#include "utils/math/MathFuncs.hpp"

Vec3::Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
Vec3::Vec3(float s) : x(s), y(s), z(s) {}
Vec3::Vec3() : x(0), y(0), z(0) {}

Vec3::operator Vec2() const { return Vec2(x, y); }
Vec3::operator Vec4() const { return Vec4(x, y, z, 1); }

Vec3 Vec3::operator-() const { return Vec3(-x, -y, -z); }

Vec3 Vec3::operator+(const Vec3& other) const
{
    return Vec3(x + other.x, y + other.y, z + other.z);
}
Vec3 Vec3::operator-(const Vec3& other) const
{
    return Vec3(x - other.x, y - other.y, z - other.z);
}
Vec3 Vec3::operator*(const Vec3& other) const
{
    return Vec3(x * other.x, y * other.y, z * other.z);
}
Vec3 Vec3::operator/(const Vec3& other) const
{
    return Vec3(x / other.x, y / other.y, z / other.z);
}

Vec3 Vec3::operator*(float other) const { return Vec3(x * other, y * other, z * other); }
Vec3 Vec3::operator/(float other) const { return Vec3(x / other, y / other, z / other); }
Vec3 operator*(float left, const Vec3& right)
{
    return Vec3(left * right.x, left * right.y, left * right.z);
}

Vec3& Vec3::operator+=(const Vec3& other)
{
    *this = *this + other;
    return *this;
}
Vec3& Vec3::operator-=(const Vec3& other)
{
    *this = *this - other;
    return *this;
}
Vec3& Vec3::operator*=(const Vec3& other)
{
    *this = *this * other;
    return *this;
}
Vec3& Vec3::operator/=(const Vec3& other)
{
    *this = *this / other;
    return *this;
}

Vec3& Vec3::operator*=(float other)
{
    *this = *this * other;
    return *this;
}
Vec3& Vec3::operator/=(float other)
{
    *this = *this / other;
    return *this;
}

bool Vec3::operator==(const Vec3& other) const
{
    return x == other.x && y == other.y && z == other.z;
}
bool Vec3::operator!=(const Vec3& other) const { return !(*this == other); }

float Vec3::magnitude() const { return Math::sqrt(x * x + y * y + z * z); }
float Vec3::sqrMagnitude() const { return x * x + y * y; }
Vec3 Vec3::normalized() const { return *this / magnitude(); }
void Vec3::normalize() { *this /= magnitude(); }

float Vec3::sqrDistanceTo(const Vec3& other) const { return (*this - other).sqrMagnitude(); }
float Vec3::distanceTo(const Vec3& other) const { return (*this - other).magnitude(); }
Vec3 Vec3::directionTo(const Vec3& other) const { return (other - *this).normalized(); }

Vec3 Vec3::abs() const { return Vec3(Math::abs(x), Math::abs(y), Math::abs(z)); }
Vec3 Vec3::round() const { return Vec3(Math::round(x), Math::round(y), Math::round(z)); }
Vec3 Vec3::floor() const { return Vec3(Math::floor(x), Math::floor(y), Math::floor(z)); }
Vec3 Vec3::ceil() const { return Vec3(Math::ceil(x), Math::ceil(y), Math::ceil(z)); }

Vec3 Vec3::magnitudeClamped(float length) const
{
    if (sqrMagnitude() <= length * length) return *this;
    return normalized() * length;
}
void Vec3::clampMagnitude(float length)
{
    if (sqrMagnitude() > length * length) *this = normalized() * length;
}

Vec3 Vec3::reflected(const Vec3& normal) const { return *this - 2 * dot(*this, normal) * normal; }
void Vec3::reflect(const Vec3& normal) { *this = reflected(normal); }

Vec3 Vec3::min(const Vec3& a, const Vec3& b)
{
    return Vec3(Math::min(a.x, b.x), Math::min(a.y, b.y), Math::min(a.z, b.z));
}
Vec3 Vec3::max(const Vec3& a, const Vec3& b)
{
    return Vec3(Math::max(a.x, b.x), Math::max(a.y, b.y), Math::max(a.z, b.z));
}

float Vec3::dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float Vec3::dotN(const Vec3& a, const Vec3& b) { return dot(a.normalized(), b.normalized()); }

Vec3 Vec3::cross(const Vec3& a, const Vec3& b)
{
    return Vec3(a.y * b.z - a.z * b.y, a.z - b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
Vec3 Vec3::crossN(const Vec3& a, const Vec3& b) { return cross(a.normalized(), b.normalized()); }

Vec3 Vec3::lerp(const Vec3& from, const Vec3& to, float t) { return (to - from) * t + from; }
float Vec3::inverseLerp(const Vec3& from, const Vec3& to, const Vec3& val)
{
    // doesn't use z axis maybe add it later
    if (Math::abs(from.x - to.x) > Math::abs(from.y - to.y))
        return Math::inverseLerp(from.x, to.x, val.x);
    else return Math::inverseLerp(from.y, to.y, val.y);
}
Vec3 Vec3::moveTowards(const Vec3& from, const Vec3& to, float step)
{
    if (from.sqrDistanceTo(to) > step * step) return from + from.directionTo(to) * step;
    else return to;
}
