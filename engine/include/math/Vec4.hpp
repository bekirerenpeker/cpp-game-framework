#pragma once

#include <ostream>

class Vec2;
class Vec3;

class Vec4
{
  public:
    union {
        struct
        {
            float x, y, z, w;
        };
        struct
        {
            float r, g, b, a;
        };
    };

  public:
    Vec4(float x, float y, float z, float w = 1);
    Vec4(float s);
    Vec4();

    // conversion operators
    operator Vec2() const;
    operator Vec3() const;
    // operator Color() const;

    // printing operator
    friend std::ostream& operator<<(std::ostream& stream, const Vec4& v);

    // arithmetic operators
    Vec4 operator-() const;

    Vec4 operator+(const Vec4& other) const;
    Vec4 operator-(const Vec4& other) const;
    Vec4 operator*(const Vec4& other) const;
    Vec4 operator/(const Vec4& other) const;

    Vec4 operator*(float other) const;
    Vec4 operator/(float other) const;
    friend Vec4 operator*(float left, const Vec4& right);

    Vec4& operator+=(const Vec4& other);
    Vec4& operator-=(const Vec4& other);
    Vec4& operator*=(const Vec4& other);
    Vec4& operator/=(const Vec4& other);

    Vec4& operator*=(float other);
    Vec4& operator/=(float other);

    // compariosn operators
    bool operator==(const Vec4& other) const;
    bool operator!=(const Vec4& other) const;
};
