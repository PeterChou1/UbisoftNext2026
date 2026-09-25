#include "Vec4.h"

#include "stdafx.h"

#include <cassert>
#include <math.h>

Vec4::Vec4()
    : X(0)
    , Y(0)
    , Z(0)
    , W(0)
{
}

Vec4::Vec4(const Vec3& rhs)
    : X(rhs.X)
    , Y(rhs.Y)
    , Z(rhs.Z)
    , W(1)
{
}

Vec4::Vec4(const float value)
    : X(value)
    , Y(value)
    , Z(value)
    , W(value)
{
}

Vec4::Vec4(const Vec4& rhs)
    : X(rhs.X)
    , Y(rhs.Y)
    , Z(rhs.Z)
    , W(rhs.W)
{
}

Vec4::Vec4(float X, float Y, float Z, float W)
    : X(X)
    , Y(Y)
    , Z(Z)
    , W(W)
{
}

Vec4& Vec4::operator=(const Vec4& rhs) = default;

Vec4 Vec4::operator+(const Vec4& rhs) const
{
    Vec4 temp;
    temp.X = X + rhs.X;
    temp.Y = Y + rhs.Y;
    temp.Z = Z + rhs.Z;
    temp.W = W + rhs.W;
    return temp;
}

const Vec4& Vec4::operator*=(const Vec4& rhs)
{
    X *= rhs.X;
    Y *= rhs.Y;
    Z *= rhs.Z;
    W *= rhs.W;
    return *this;
}

Vec4 Vec4::operator*(float rhs) const
{
    Vec4 temp;
    temp.X = X * rhs;
    temp.Y = Y * rhs;
    temp.Z = Z * rhs;
    temp.W = W * rhs;
    return temp;
}

float Vec4::operator[](int idx) const
{
    assert(idx >= 0 && idx < 4);
    return (&X)[idx];
}

float& Vec4::operator[](const int idx)
{
    assert(idx >= 0 && idx < 4);
    return (&X)[idx];
}

float Vec4::Dot(const Vec4& rhs) const
{
    float xx = X * rhs.X;
    float yy = Y * rhs.Y;
    float zz = Z * rhs.Z;
    float ww = W * rhs.W;
    return (xx + yy + zz + ww);
}

Vec3 Vec4::ToVec3() const
{
    if (W != 0)
    {
        return Vec3(X / W, Y / W, Z / W);
    }
    return Vec3(X, Y, Z);
}
