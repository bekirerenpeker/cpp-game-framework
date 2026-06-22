#pragma once

#include "utils/math/Vec3.hpp"
#include "utils/math/Vec4.hpp"

namespace Engine {

class Mat4
{
  public:
    float vals[4][4];   // vals[col][row]

  public:
    Mat4();
    Mat4(float v);

    Mat4& identity();

    Mat4& translate(Vec3 offset);
    Mat4& scale(Vec3 scale);
    Mat4& rotate(float radians);

    static Mat4 translateMat(Vec3 offset);
    static Mat4 scaleMat(Vec3 scale);
    static Mat4 rotateMat(float radians);
    static Mat4 ortho(float left, float right, float bottom, float top, float zNear, float zFar);

    Mat4 operator*(const Mat4& other) const;
    Vec4 operator*(const Vec4& other) const;
    Mat4 operator*(float other) const;
    Mat4& operator*=(const Mat4& other);
    Mat4& operator*=(float other);

    const float* operator[](int row) const { return vals[row]; }
    float* operator[](int row) { return vals[row]; }
};

}   // namespace Engine
