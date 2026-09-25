//---------------------------------------------------------------------------------
// Mat4.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a 4d Matrix
//
#pragma once
#include "Mat3.h"
#include "Vec4.h"

class Mat4
{
  public:
    Mat4() {}

    Mat4(const Vec4& row0, const Vec4& row1, const Vec4& row2, const Vec4& row3);

    float Determinant() const;
    Mat4 Inverse() const;
    /// fast inverse specifically for affine matrix
    Mat4 AffineInverse() const;
    Mat3 Minor(int i, int j) const;
    float Cofactor(int i, int j) const;
    /// Construct An Open GL Perspective Matrix
    void PerspectiveOpenGL(float fovy, float aspect_ratio, float near, float far);
    void OrthogonalOpenGL(float bottom, float left, float top, float right, float near, float far);
    Vec4 operator*(const Vec4& rhs) const;
    Vec3 operator*(const Vec3& rhs) const;
    Vec4 operator[](const int i) const;
    Vec4& operator[](const int i);

    const Mat4& operator*=(const float rhs);

    Vec4 Rows[4];
};
