//---------------------------------------------------------------------------------
// Vec2.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a 2d Vector
//
#pragma once

#include <string>

class Vec2
{
  public:
    Vec2()
        : X(0)
        , Y(0)
    {
    }

    Vec2(float X, float Y)
        : X(X)
        , Y(Y)
    {
    }

    bool operator==(const Vec2& rhs) const { return X == rhs.X && Y == rhs.Y; }

    Vec2 operator+(const Vec2& rhs) const { return Vec2(X + rhs.X, Y + rhs.Y); }
    Vec2 operator-(const Vec2& rhs) const { return Vec2(X - rhs.X, Y - rhs.Y); }
    Vec2 operator*(float rhs) const { return Vec2(X * rhs, Y * rhs); }
    Vec2 operator/(float rhs) const { return Vec2(X / rhs, Y / rhs); }

    const Vec2& operator+=(const Vec2& rhs);
    const Vec2& operator-=(const Vec2& rhs);
    const Vec2& operator*=(float rhs);

    /// Leaves a zero-length vector unchanged
    const Vec2& Normalize();

    /// Cross product with a vector along Z of length rhs (a perpendicular)
    Vec2 Cross(float rhs) const { return {Y * rhs, X * -rhs}; }
    /// Z component of the 3d cross product
    float Cross(const Vec2& rhs) const { return X * rhs.Y - Y * rhs.X; }
    float Dot(const Vec2& rhs) const { return X * rhs.X + Y * rhs.Y; }
    float GetMagnitude() const;
    float GetMagnitudeSquared() const { return X * X + Y * Y; }

    std::string ToString() const;

    float X;
    float Y;
};
