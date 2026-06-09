#pragma once

#include <ostream>

class Vec2;
class Vec4;

class Vec3
{
  public:
    union {
        struct
        {
            float x, y, z;
        };
        struct
        {
            float r, g, b;
        };
    };

  public:
    Vec3(float x, float y, float z);
    Vec3(float s);
    Vec3();

    // conversion operators
    operator Vec2() const;
    operator Vec4() const;

    // printing operator
    friend std::ostream& operator<<(std::ostream& stream, const Vec3& v);

    // arithmetic operators
    Vec3 operator-() const;

    Vec3 operator+(const Vec3& other) const;
    Vec3 operator-(const Vec3& other) const;
    Vec3 operator*(const Vec3& other) const;
    Vec3 operator/(const Vec3& other) const;

    Vec3 operator*(float other) const;
    Vec3 operator/(float other) const;
    friend Vec3 operator*(float left, const Vec3& right);

    Vec3& operator+=(const Vec3& other);
    Vec3& operator-=(const Vec3& other);
    Vec3& operator*=(const Vec3& other);
    Vec3& operator/=(const Vec3& other);

    Vec3& operator*=(float other);
    Vec3& operator/=(float other);

    // compariosn operators
    bool operator==(const Vec3& other) const;
    bool operator!=(const Vec3& other) const;

    // member functions
    float magnitude() const;
    float sqrMagnitude() const;
    Vec3 normalized() const;
    void normalize();

    Vec3 abs() const;
    Vec3 round() const;
    Vec3 floor() const;
    Vec3 ceil() const;

    Vec3 magnitudeClamped(float length) const;
    void clampMagnitude(float length);

    // static functions
    static float sqrDist(const Vec3& a, const Vec3& b);
    static float dist(const Vec3& a, const Vec3& b);
    static Vec3 dir(const Vec3& from, const Vec3& to);

    static Vec3 min(const Vec3& a, const Vec3& b);
    static Vec3 max(const Vec3& a, const Vec3& b);

    static float dot(const Vec3& a, const Vec3& b);
    static float dotN(const Vec3& a, const Vec3& b);   // this function normalizes the vectors

    static Vec3 cross(const Vec3& a, const Vec3& b);
    static Vec3 crossN(const Vec3& a, const Vec3& b);   // this function normalizes the vectors

    static Vec3 reflect(const Vec3& ray, const Vec3& normal);

    static Vec3 lerp(const Vec3& from, const Vec3& to, float t);
    static float inverseLerp(const Vec3& from, const Vec3& to, const Vec3& val);
    static Vec3 moveTowards(const Vec3& from, const Vec3& to, float step);
};

#define VEC3_RIGHT   Vec3(1, 0, 0)
#define VEC3_LEFT    Vec3(-1, 0, 0)
#define VEC3_UP      Vec3(0, 1, 0)
#define VEC3_DOWN    Vec3(0, -1, 0)
#define VEC3_FORWARD Vec3(0, 0, 1)
#define VEC3_BACK    Vec3(0, 0, -1)
#define VEC3_ZERO    Vec3(0, 0, 0)
#define VEC3_ONE     Vec3(1, 1, 1)
