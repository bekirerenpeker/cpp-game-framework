#include "math/Mat4.hpp"
#include "math/Math.hpp"

Mat4::Mat4() { identity(); }
Mat4::Mat4(float v)
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) vals[r][c] = v;
}

Mat4& Mat4::identity()
{
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) vals[r][c] = (r == c) ? 1 : 0;
    return *this;
}

Mat4& Mat4::translate(Vec3 offset)
{
    *this *= translateMat(offset);
    return *this;
}
Mat4& Mat4::scale(Vec3 scale)
{
    *this *= scaleMat(scale);
    return *this;
}
Mat4& Mat4::rotate(float radians)
{
    *this *= rotateMat(radians);
    return *this;
}

Mat4 Mat4::translateMat(Vec3 offset)
{
    Mat4 mat;
    mat[3][0] += offset.x;
    mat[3][1] += offset.y;
    mat[3][2] += offset.z;
    return mat;
}
Mat4 Mat4::scaleMat(Vec3 scale)
{
    Mat4 mat;
    mat[0][0] *= scale.x;
    mat[1][1] *= scale.y;
    return mat;
}
Mat4 Mat4::rotateMat(float radians)
{
    Mat4 mat;
    mat[0][0] = Math::cos(radians);
    mat[0][1] = -Math::sin(radians);
    mat[1][0] = Math::sin(radians);
    mat[1][1] = Math::cos(radians);
    return mat;
}
Mat4 Mat4::ortho(float left, float right, float bottom, float top, float zNear, float zFar)
{
    Mat4 mat;
    mat[0][0] = 2 / (right - left);
    mat[1][1] = 2 / (top - bottom);
    mat[2][2] = -2 / (zFar - zNear);
    mat[3][0] = -(right + left) / (right - left);
    mat[3][1] = -(top + bottom) / (top - bottom);
    mat[3][2] = -(zFar + zNear) / (zFar - zNear);
    return mat;
}

Mat4 Mat4::operator*(float other) const
{
    Mat4 out;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) out.vals[r][c] = vals[r][c] * other;
    return out;
}

Vec4 Mat4::operator*(const Vec4& other) const
{
    return Vec4(
        vals[0][0] * other.x + vals[1][0] * other.y + vals[2][0] * other.z + vals[3][0] * other.w,
        vals[0][1] * other.x + vals[1][1] * other.y + vals[2][1] * other.z + vals[3][1] * other.w,
        vals[0][2] * other.x + vals[1][2] * other.y + vals[2][2] * other.z + vals[3][2] * other.w,
        vals[0][3] * other.x + vals[1][3] * other.y + vals[2][3] * other.z + vals[3][3] * other.w
    );
}

Mat4 Mat4::operator*(const Mat4& other) const
{
    Mat4 out;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            out[c][r] = vals[0][r] * other[c][0] + vals[1][r] * other[c][1] +
                        vals[2][r] * other[c][2] + vals[3][r] * other[c][3];
    return out;
}

Mat4& Mat4::operator*=(float other)
{
    *this = *this * other;
    return *this;
}
Mat4& Mat4::operator*=(const Mat4& other)
{
    *this = *this * other;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const Mat4& mat)
{
    for (int r = 0; r < 4; r++) {
        os << ((r == 0) ? "Mat4(" : "     ");
        for (int c = 0; c < 4; c++) os << mat[r][c] << ((r * c == 9) ? ")" : ", ");
        if (r != 3) os << "\n";
    }
    return os;
}
