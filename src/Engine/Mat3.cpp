#include "Mat3.h"

#include "Quat.h"

#include <cassert>

Vec3& Mat3::operator[](const int i)
{
    assert(i >= 0 && i < 3);
    return Rows[i];
}

Mat3 Mat3::FromQuat(const Quat& q)
{
    const float xx = q.X * q.X, xy = q.X * q.Y, xz = q.X * q.Z, xw = q.X * q.W;
    const float yy = q.Y * q.Y, yz = q.Y * q.Z, yw = q.Y * q.W;
    const float zz = q.Z * q.Z, zw = q.Z * q.W;

    Mat3 m;
    m[0][0] = 1.0f - 2.0f * (yy + zz);
    m[0][1] = 2 * (xy - zw);
    m[0][2] = 2 * (xz + yw);

    m[1][0] = 2 * (xy + zw);
    m[1][1] = 1 - 2 * (xx + zz);
    m[1][2] = 2 * (yz - xw);

    m[2][0] = 2 * (xz - yw);
    m[2][1] = 2 * (yz + xw);
    m[2][2] = 1 - 2 * (xx + yy);
    return m;
}

float Mat3::Determinant() const
{
    const float i = Rows[0][0] * (Rows[1][1] * Rows[2][2] - Rows[1][2] * Rows[2][1]);
    const float j = Rows[0][1] * (Rows[1][0] * Rows[2][2] - Rows[1][2] * Rows[2][0]);
    const float k = Rows[0][2] * (Rows[1][0] * Rows[2][1] - Rows[1][1] * Rows[2][0]);
    return i - j + k;
}
