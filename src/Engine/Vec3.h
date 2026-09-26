//---------------------------------------------------------------------------------
// Vec3.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a 3d Vector
//
#pragma once

#include <cassert>
#include <string>

class Vec3
{
  public:
    Vec3()
        : X(0)
        , Y(0)
        , Z(0)
    {
    }

    Vec3(float X, float Y, float Z)
        : X(X)
        , Y(Y)
        , Z(Z)
    {
    }

    bool operator==(const Vec3& rhs) const { return X == rhs.X && Y == rhs.Y && Z == rhs.Z; }

    Vec3 operator+(const Vec3& rhs) const { return Vec3(X + rhs.X, Y + rhs.Y, Z + rhs.Z); }
    Vec3 operator-(const Vec3& rhs) const { return Vec3(X - rhs.X, Y - rhs.Y, Z - rhs.Z); }
    Vec3 operator*(float rhs) const { return Vec3(X * rhs, Y * rhs, Z * rhs); }
    Vec3 operator/(float rhs) const { return Vec3(X / rhs, Y / rhs, Z / rhs); }

    const Vec3& operator+=(const Vec3& rhs);
    const Vec3& operator*=(float rhs);

    float operator[](int idx) const
    {
        assert(idx >= 0 && idx < 3);
        return (&X)[idx];
    }

    float& operator[](int idx)
    {
        assert(idx >= 0 && idx < 3);
        return (&X)[idx];
    }

    Vec3 Cross(const Vec3& rhs) const;
    float Dot(const Vec3& rhs) const { return (X * rhs.X) + (Y * rhs.Y) + (Z * rhs.Z); }
    /// Leaves a zero-length vector unchanged
    Vec3& Normalize();
    float GetMagnitude() const;
    float GetLengthSqr() const { return Dot(*this); }

    /// False if a component is NaN or infinite
    bool IsValid() const;

    std::string ToString() const;

    float X;
    float Y;
    float Z;
};
