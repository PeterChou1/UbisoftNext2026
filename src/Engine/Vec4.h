//---------------------------------------------------------------------------------
// Vec4.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a 4d Vector
//
#pragma once
#include "Vec3.h"

class Vec4
{
  public:
    Vec4()
        : X(0)
        , Y(0)
        , Z(0)
        , W(0)
    {
    }

    /// Homogeneous point (W = 1)
    Vec4(const Vec3& rhs)
        : X(rhs.X)
        , Y(rhs.Y)
        , Z(rhs.Z)
        , W(1)
    {
    }

    Vec4(float value)
        : X(value)
        , Y(value)
        , Z(value)
        , W(value)
    {
    }

    Vec4(float X, float Y, float Z, float W)
        : X(X)
        , Y(Y)
        , Z(Z)
        , W(W)
    {
    }

    Vec4 operator+(const Vec4& rhs) const
    {
        return Vec4(X + rhs.X, Y + rhs.Y, Z + rhs.Z, W + rhs.W);
    }
    Vec4 operator*(float rhs) const { return Vec4(X * rhs, Y * rhs, Z * rhs, W * rhs); }
    /// Component-wise
    const Vec4& operator*=(const Vec4& rhs);

    float operator[](int idx) const
    {
        assert(idx >= 0 && idx < 4);
        return (&X)[idx];
    }

    float& operator[](int idx)
    {
        assert(idx >= 0 && idx < 4);
        return (&X)[idx];
    }

    float Dot(const Vec4& rhs) const { return X * rhs.X + Y * rhs.Y + Z * rhs.Z + W * rhs.W; }
    /// Perspective division (skipped when W is 0)
    Vec3 ToVec3() const;

    float X, Y, Z, W;
};
