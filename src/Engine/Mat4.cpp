#include "Mat4.h"

#include "stdafx.h"

#include <cassert>
#include <math.h>

Mat4::Mat4(const Vec4& row0, const Vec4& row1, const Vec4& row2, const Vec4& row3)
{
    Rows[0] = row0;
    Rows[1] = row1;
    Rows[2] = row2;
    Rows[3] = row3;
}

const Mat4& Mat4::operator*=(const float rhs)
{
    Rows[0] *= rhs;
    Rows[1] *= rhs;
    Rows[2] *= rhs;
    Rows[3] *= rhs;
    return *this;
}

float Mat4::Determinant() const
{
    float det = 0.0f;
    float sign = 1.0f;
    for (int j = 0; j < 4; j++)
    {
        Mat3 minor = Minor(0, j);
        det += Rows[0][j] * minor.Determinant() * sign;
        sign = -sign;
    }
    return det;
}

Mat4 Mat4::Inverse() const
{
    Mat4 inv;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            inv.Rows[j][i] = Cofactor(i, j);
        }
    }
    float det = Determinant();
    // fail on non-invertible matrix
    assert(det != 0.0f && "matrix not invertible");
    float invDet = 1.0f / det;
    inv *= invDet;
    return inv;
}

Mat4 Mat4::AffineInverse() const
{
    Mat4 matrix;
    matrix[0][0] = (*this)[0][0];
    matrix[1][0] = (*this)[0][1];
    matrix[2][0] = (*this)[0][2];
    matrix[3][0] = 0.0f;
    matrix[0][1] = (*this)[1][0];
    matrix[1][1] = (*this)[1][1];
    matrix[2][1] = (*this)[1][2];
    matrix[3][1] = 0.0f;
    matrix[0][2] = (*this)[2][0];
    matrix[1][2] = (*this)[2][1];
    matrix[2][2] = (*this)[2][2];
    matrix[3][2] = 0.0f;
    matrix[0][3] = -((*this)[0][3] * matrix[0][0] + (*this)[1][3] * matrix[0][1] +
                     (*this)[2][3] * matrix[0][2]);
    matrix[1][3] = -((*this)[0][3] * matrix[1][0] + (*this)[1][3] * matrix[1][1] +
                     (*this)[2][3] * matrix[1][2]);
    matrix[2][3] = -((*this)[0][3] * matrix[2][0] + (*this)[1][3] * matrix[2][1] +
                     (*this)[2][3] * matrix[2][2]);
    matrix[3][3] = 1.0f;
    return matrix;
}

Mat3 Mat4::Minor(const int i, const int j) const
{
    Mat3 minor;
    int yy = 0;
    for (int y = 0; y < 4; y++)
    {
        if (y == j)
        {
            continue;
        }
        int xx = 0;
        for (int x = 0; x < 4; x++)
        {
            if (x == i)
            {
                continue;
            }
            minor.Rows[xx][yy] = Rows[x][y];
            xx++;
        }
        yy++;
    }
    return minor;
}

float Mat4::Cofactor(const int i, const int j) const
{
    const Mat3 minor = Minor(i, j);
    const float C = float(pow(-1, i + 1 + j + 1)) * minor.Determinant();
    return C;
}

/// Code Based on:
/// Based on
/// https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/opengl-perspective-projection-matrix.html
void Mat4::PerspectiveOpenGL(float fovy, float aspect_ratio, float near, float far)
{
    const float pi = acosf(-1.0f);
    const float scale = tanf(fovy * 0.5f * pi / 180.0f) * near;
    float right = aspect_ratio * scale;
    float left = -right;
    float top = scale;
    float bottom = -top;

    Rows[0] = Vec4(2 * near / (right - left), 0, (right + left) / (right - left), 0);
    Rows[1] = Vec4(0, 2 * near / (top - bottom), (top + bottom) / (top - bottom), 0);
    Rows[2] = Vec4(0, 0, -(far + near) / (far - near), (-2.0f * far * near) / (far - near));
    Rows[3] = Vec4(0, 0, -1, 0);
}

void Mat4::OrthogonalOpenGL(float bottom, float left, float top, float right, float near, float far)
{

    Rows[0] = Vec4(2 / (right - left), 0, 0, -(right + left) / (right - left));
    Rows[1] = Vec4(0, 2 / (top - bottom), 0, -(top + bottom) / (top - bottom));
    Rows[2] = Vec4(0, 0, -2.0f / (far - near), -(far + near) / (far - near));
    Rows[3] = Vec4(0, 0, 0, 1);
}

Vec4 Mat4::operator*(const Vec4& rhs) const
{
    Vec4 tmp;
    tmp[0] = Rows[0].Dot(rhs);
    tmp[1] = Rows[1].Dot(rhs);
    tmp[2] = Rows[2].Dot(rhs);
    tmp[3] = Rows[3].Dot(rhs);
    return tmp;
}

Vec3 Mat4::operator*(const Vec3& rhs) const
{
    return (*this * Vec4(rhs)).ToVec3();
}

Vec4 Mat4::operator[](const int i) const
{
    assert(i >= 0 && i < 4);
    return Rows[i];
}

Vec4& Mat4::operator[](const int i)
{
    assert(i >= 0 && i < 4);
    return Rows[i];
}
