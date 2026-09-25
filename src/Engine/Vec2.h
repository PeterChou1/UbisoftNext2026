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
    Vec2();
    Vec2(const Vec2& rhs);
    Vec2(float X, float Y);

    Vec2& operator=(const Vec2& rhs);

    bool operator==(const Vec2& rhs) const;

    Vec2 operator+(const Vec2& rhs) const;
    const Vec2& operator+=(const Vec2& rhs);

    Vec2 operator-(const Vec2& rhs) const;
    const Vec2& operator-=(const Vec2& rhs);

    Vec2 operator*(float rhs) const;
    Vec2 operator/(float rhs) const;

    const Vec2& operator*=(float rhs);

    const Vec2& Normalize();
    Vec2 Cross(float rhs) const;
    float Cross(const Vec2& rhs) const;
    float GetMagnitude() const;
    float GetMagnitudeSquared() const;

    std::string ToString() const;

    float Dot(const Vec2& rhs) const { return X * rhs.X + Y * rhs.Y; }

    const float* ToPtr() const { return &X; }

    float X;
    float Y;
};
