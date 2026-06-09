#include "math/Vec4.hpp"
#include "math/Vec2.hpp"
#include "math/Vec3.hpp"

Vec4::Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
Vec4::Vec4(float s) : x(s), y(s), z(s), w(s) {}
Vec4::Vec4() : x(0), y(0), z(0), w(0) {}

// conversion operators
Vec4::operator Vec2() const { return Vec2(x, y); }
Vec4::operator Vec3() const { return Vec3(x, y, z); }
// Vec4::operator Color() const { return Color(r, g, b, a); }

// printing operator
std::ostream& operator<<(std::ostream& stream, const Vec4& v)
{
    stream << "Vec4(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")";
    return stream;
}

// arithmetic operators
Vec4 Vec4::operator-() const { return Vec4(-x, -y, -z, -w); }

Vec4 Vec4::operator+(const Vec4& other) const
{
    return Vec4(x + other.x, y + other.y, z + other.z, w + other.w);
}
Vec4 Vec4::operator-(const Vec4& other) const
{
    return Vec4(x - other.x, y - other.y, z - other.z, w - other.w);
}
Vec4 Vec4::operator*(const Vec4& other) const
{
    return Vec4(x * other.x, y * other.y, z * other.z, w * other.w);
}
Vec4 Vec4::operator/(const Vec4& other) const
{
    return Vec4(x / other.x, y / other.y, z / other.z, w / other.w);
}

Vec4 Vec4::operator*(float other) const { return Vec4(x * other, y * other, z * other, w * other); }
Vec4 Vec4::operator/(float other) const { return Vec4(x / other, y / other, z / other, w * other); }
Vec4 operator*(float left, const Vec4& right)
{
    return Vec4(left * right.x, left * right.y, left * right.z, left * right.w);
}

Vec4& Vec4::operator+=(const Vec4& other)
{
    *this = *this + other;
    return *this;
}
Vec4& Vec4::operator-=(const Vec4& other)
{
    *this = *this - other;
    return *this;
}
Vec4& Vec4::operator*=(const Vec4& other)
{
    *this = *this * other;
    return *this;
}
Vec4& Vec4::operator/=(const Vec4& other)
{
    *this = *this / other;
    return *this;
}

Vec4& Vec4::operator*=(float other)
{
    *this = *this * other;
    return *this;
}
Vec4& Vec4::operator/=(float other)
{
    *this = *this / other;
    return *this;
}

// compariosn operators
bool Vec4::operator==(const Vec4& other) const
{
    return x == other.x && y == other.y && z == other.z && w == other.w;
}
bool Vec4::operator!=(const Vec4& other) const { return !(*this == other); }
