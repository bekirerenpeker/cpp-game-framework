#pragma once

#include <ostream>

class Vec3;
class Vec4;

class Vec2
{
  public:
    union {
        struct
        {
            float x, y;
        };
        struct
        {
            float r, g;
        };
    };

  public:
    Vec2(float x, float y);
    Vec2(float s);
    Vec2();

    // conversion operators
    operator Vec3() const;
    operator Vec4() const;

    // printing operator
    friend std::ostream& operator<<(std::ostream& stream, const Vec2& v);

    // arithmetic operators
    Vec2 operator-() const;

    Vec2 operator+(const Vec2& other) const;
    Vec2 operator-(const Vec2& other) const;
    Vec2 operator*(const Vec2& other) const;
    Vec2 operator/(const Vec2& other) const;

    Vec2 operator*(float other) const;
    Vec2 operator/(float other) const;
    friend Vec2 operator*(float left, const Vec2& right);

    Vec2& operator+=(const Vec2& other);
    Vec2& operator-=(const Vec2& other);
    Vec2& operator*=(const Vec2& other);
    Vec2& operator/=(const Vec2& other);

    Vec2& operator*=(float other);
    Vec2& operator/=(float other);

    // compariosn operators
    bool operator==(const Vec2& other) const;
    bool operator!=(const Vec2& other) const;

    // member functions
    float magnitude() const;
    float sqrMagnitude() const;
    Vec2 normalized() const;
    void normalize();

    Vec2 abs() const;
    Vec2 round() const;
    Vec2 floor() const;
    Vec2 ceil() const;
    float angle() const;

    Vec2 magnitudeClamped(float length) const;
    void clampMagnitude(float length);

    void rotateAround(Vec2 pivot, float angle);

    // static functions
    static float sqrDist(const Vec2& a, const Vec2& b);
    static float dist(const Vec2& a, const Vec2& b);
    static Vec2 dir(const Vec2& from, const Vec2& to);
    static float angle(const Vec2& from, const Vec2& to);

    static Vec2 min(const Vec2& a, const Vec2& b);
    static Vec2 max(const Vec2& a, const Vec2& b);

    static float dot(const Vec2& a, const Vec2& b);
    static float dotN(const Vec2& a, const Vec2& b);   // this function normalizes the vectors

    static Vec2 rotateAround(const Vec2& v, const Vec2& pivot, float angle);

    static Vec2 perpendicular(const Vec2& v);
    static Vec2 reflect(const Vec2& ray, const Vec2& normal);

    static Vec2 lerp(const Vec2& from, const Vec2& to, float t);
    static float inverseLerp(const Vec2& from, const Vec2& to, const Vec2& val);
    static Vec2 moveTowards(const Vec2& from, const Vec2& to, float step);
};

#define VEC2_RIGHT Vec2(1, 0)
#define VEC2_LEFT  Vec2(-1, 0)
#define VEC2_UP    Vec2(0, 1)
#define VEC2_DOWN  Vec2(0, -1)
#define VEC2_ZERO  Vec2(0, 0)
#define VEC2_ONE   Vec2(1, 1)
