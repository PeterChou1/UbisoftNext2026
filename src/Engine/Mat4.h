//---------------------------------------------------------------------------------
// Mat4.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a 4d Matrix (row major, column vectors)
//
#pragma once
#include "Mat3.h"
#include "Vec4.h"

class Mat4
{
  public:
    Mat4() {}

    Mat4(const Vec4& row0, const Vec4& row1, const Vec4& row2, const Vec4& row3)
        : Rows{row0, row1, row2, row3}
    {
    }

    float Determinant() const;
    Mat4 Inverse() const;
    /// Fast inverse of an affine matrix whose 3x3 part is a pure rotation
    Mat4 AffineInverse() const;
    /// OpenGL perspective projection (fovy in degrees)
    void PerspectiveOpenGL(float fovy, float aspectRatio, float near, float far);
    void OrthogonalOpenGL(float bottom, float left, float top, float right, float near, float far);

    Vec4 operator*(const Vec4& rhs) const
    {
        return Vec4(Rows[0].Dot(rhs), Rows[1].Dot(rhs), Rows[2].Dot(rhs), Rows[3].Dot(rhs));
    }

    /// Transforms a point (W = 1) and divides by the resulting W
    Vec3 operator*(const Vec3& rhs) const { return (*this * Vec4(rhs)).ToVec3(); }

    Vec4 operator[](int i) const
    {
        assert(i >= 0 && i < 4);
        return Rows[i];
    }

    Vec4& operator[](int i)
    {
        assert(i >= 0 && i < 4);
        return Rows[i];
    }

    const Mat4& operator*=(float rhs);

    Vec4 Rows[4];

  private:
    /// The 3x3 matrix left after removing row i and column j (stored
    /// transposed, which keeps its determinant)
    Mat3 Minor(int i, int j) const;
    float Cofactor(int i, int j) const;
};
