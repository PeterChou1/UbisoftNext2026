#include "Vec2.h"

#include "stdafx.h"

#include <cassert>
#include <math.h>

Vec2::Vec2()
    : X(0)
    , Y(0)
{
}

Vec2::Vec2(const Vec2& rhs)
    : X(rhs.X)
    , Y(rhs.Y)
{
}

Vec2::Vec2(float X, float Y)
    : X(X)
    , Y(Y)
{
}

Vec2& Vec2::operator=(const Vec2& rhs) = default;

bool Vec2::operator==(const Vec2& rhs) const
{
    // Exact, like Vec3 (this used to return false for equal vectors)
    return X == rhs.X && Y == rhs.Y;
}

Vec2 Vec2::operator+(const Vec2& rhs) const
{
    Vec2 temp;
    temp.X = X + rhs.X;
    temp.Y = Y + rhs.Y;
    return temp;
}

const Vec2& Vec2::operator+=(const Vec2& rhs)
{
    X += rhs.X;
    Y += rhs.Y;
    return *this;
}

const Vec2& Vec2::operator-=(const Vec2& rhs)
{
    X -= rhs.X;
    Y -= rhs.Y;
    return *this;
}

Vec2 Vec2::operator-(const Vec2& rhs) const
{
    Vec2 temp;
    temp.X = X - rhs.X;
    temp.Y = Y - rhs.Y;
    return temp;
}

Vec2 Vec2::operator*(const float rhs) const
{
    Vec2 temp;
    temp.X = X * rhs;
    temp.Y = Y * rhs;
    return temp;
}

Vec2 Vec2::operator/(float rhs) const
{
    return Vec2(X / rhs, Y / rhs);
}

const Vec2& Vec2::operator*=(const float rhs)
{
    X *= rhs;
    Y *= rhs;
    return *this;
}

const Vec2& Vec2::Normalize()
{
    float mag = GetMagnitude();
    float invMag = 1.0f / mag;
    if (0.0f * invMag == 0.0f * invMag)
    {
        X = X * invMag;
        Y = Y * invMag;
    }
    return *this;
}

float Vec2::Cross(const Vec2& rhs) const
{
    return X * rhs.Y - Y * rhs.X;
}

Vec2 Vec2::Cross(float rhs) const
{
    return {Y * rhs, X * -rhs};
}

float Vec2::GetMagnitude() const
{
    float mag = X * X + Y * Y;
    mag = sqrtf(mag);
    return mag;
}

float Vec2::GetMagnitudeSquared() const
{
    return X * X + Y * Y;
}

std::string Vec2::ToString() const
{
    return "{" + std::to_string(X) + "," + std::to_string(Y) + "}";
}
