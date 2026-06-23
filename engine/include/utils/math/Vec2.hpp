#pragma once

#include "core/logging/LoggerMacros.hpp"

namespace Engine {

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

    operator Vec3() const;
    operator Vec4() const;

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

    bool operator==(const Vec2& other) const;
    bool operator!=(const Vec2& other) const;

    float magnitude() const;
    float sqrMagnitude() const;
    Vec2 normalized() const;
    void normalize();

    float sqrDistanceTo(const Vec2& other) const;
    float distanceTo(const Vec2& other) const;
    Vec2 directionTo(const Vec2& other) const;
    float angleTo(const Vec2& other) const;

    Vec2 abs() const;
    Vec2 round() const;
    Vec2 floor() const;
    Vec2 ceil() const;
    float angle() const;

    Vec2 magnitudeClamped(float length) const;
    void clampMagnitude(float length);

    Vec2 rotatedAround(Vec2 pivot, float angle) const;
    void rotateAround(Vec2 pivot, float angle);

    Vec2 perpendicular() const;
    Vec2 reflected(const Vec2& normal) const;
    void reflect(const Vec2& normal);

    static Vec2 min(const Vec2& a, const Vec2& b);
    static Vec2 max(const Vec2& a, const Vec2& b);

    static float dot(const Vec2& a, const Vec2& b);
    // normalizes the vectors before calculating the dot product
    static float dotN(const Vec2& a, const Vec2& b);

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

}   // namespace Engine

DEFINE_TYPE_FORMATTER(Engine::Vec2, "Vec2({}, {})", obj.x, obj.y);
