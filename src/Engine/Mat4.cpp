#include "Mat4.h"

#include <cmath>

const Mat4& Mat4::operator*=(float rhs)
{
    for (Vec4& row : Rows)
        row *= rhs;
    return *this;
}

float Mat4::Determinant() const
{
    float det = 0.0f;
    float sign = 1.0f;
    for (int j = 0; j < 4; j++)
    {
        det += Rows[0][j] * Minor(0, j).Determinant() * sign;
        sign = -sign;
    }
    return det;
}

Mat4 Mat4::Inverse() const
{
    // Adjugate (transposed cofactors) divided by the determinant
    Mat4 inv;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
            inv.Rows[j][i] = Cofactor(i, j);
    }
    float det = Determinant();
    assert(det != 0.0f && "matrix not invertible");
    inv *= 1.0f / det;
    return inv;
}

Mat4 Mat4::AffineInverse() const
{
    // Transposed rotation, translation rotated back and negated
    Mat4 inv;
    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
            inv.Rows[r][c] = Rows[c][r];
    }
    for (int r = 0; r < 3; r++)
    {
        inv.Rows[r][3] = -(Rows[0][3] * inv.Rows[r][0] + Rows[1][3] * inv.Rows[r][1] +
                           Rows[2][3] * inv.Rows[r][2]);
    }
    inv.Rows[3][3] = 1.0f;
    return inv;
}

Mat3 Mat4::Minor(int i, int j) const
{
    Mat3 minor;
    int yy = 0;
    for (int y = 0; y < 4; y++)
    {
        if (y == j)
            continue;
        int xx = 0;
        for (int x = 0; x < 4; x++)
        {
            if (x == i)
                continue;
            minor.Rows[xx][yy] = Rows[x][y];
            xx++;
        }
        yy++;
    }
    return minor;
}

float Mat4::Cofactor(int i, int j) const
{
    const float sign = (i + j) % 2 == 0 ? 1.0f : -1.0f;
    return sign * Minor(i, j).Determinant();
}

// Based on
// https://www.scratchapixel.com/lessons/3d-basic-rendering/perspective-and-orthographic-projection-matrix/opengl-perspective-projection-matrix.html
void Mat4::PerspectiveOpenGL(float fovy, float aspectRatio, float near, float far)
{
    const float pi = acosf(-1.0f);
    const float scale = tanf(fovy * 0.5f * pi / 180.0f) * near;
    float right = aspectRatio * scale;
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
