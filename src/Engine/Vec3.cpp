#include "Vec3.h"

#include "stdafx.h"

#include <cassert>

Vec3::Vec3()
    : X(0)
    , Y(0)
    , Z(0)
{
}

Vec3::Vec3(const Vec3& rhs)
    : X(rhs.X)
    , Y(rhs.Y)
    , Z(rhs.Z)
{
}

Vec3::Vec3(float X, float Y, float Z)
    : X(X)
    , Y(Y)
    , Z(Z)
{
}

Vec3& Vec3::operator=(const Vec3& rhs) = default;

bool Vec3::operator==(const Vec3& rhs) const
{
    if (X != rhs.X)
    {
        return false;
    }
    if (Y != rhs.Y)
    {
        return false;
    }
    if (Z != rhs.Z)
    {
        return false;
    }
    return true;
}

Vec3 Vec3::operator+(const Vec3& rhs) const
{
    Vec3 temp;
    temp.X = X + rhs.X;
    temp.Y = Y + rhs.Y;
    temp.Z = Z + rhs.Z;
    return temp;
}

const Vec3& Vec3::operator+=(const Vec3& rhs)
{
    X += rhs.X;
    Y += rhs.Y;
    Z += rhs.Z;
    return *this;
}

Vec3 Vec3::operator-(const Vec3& rhs) const
{
    Vec3 temp;
    temp.X = X - rhs.X;
    temp.Y = Y - rhs.Y;
    temp.Z = Z - rhs.Z;
    return temp;
}

Vec3 Vec3::operator*(const float rhs) const
{
    Vec3 temp;
    temp.X = X * rhs;
    temp.Y = Y * rhs;
    temp.Z = Z * rhs;
    return temp;
}

Vec3 Vec3::operator/(const float rhs) const
{
    Vec3 temp;
    temp.X = X / rhs;
    temp.Y = Y / rhs;
    temp.Z = Z / rhs;
    return temp;
}

const Vec3& Vec3::operator*=(const float rhs)
{
    X *= rhs;
    Y *= rhs;
    Z *= rhs;
    return *this;
}

float Vec3::operator[](const int idx) const
{
    assert(idx >= 0 && idx < 3);
    return (&X)[idx];
}

float& Vec3::operator[](const int idx)
{
    assert(idx >= 0 && idx < 3);
    return (&X)[idx];
}

Vec3 Vec3::Cross(const Vec3& rhs) const
{
    Vec3 temp;
    temp.X = (Y * rhs.Z) - (rhs.Y * Z);
    temp.Y = (rhs.X * Z) - (X * rhs.Z);
    temp.Z = (X * rhs.Y) - (rhs.X * Y);
    return temp;
}

float Vec3::Dot(const Vec3& rhs) const
{
    float temp = (X * rhs.X) + (Y * rhs.Y) + (Z * rhs.Z);
    return temp;
}

Vec3& Vec3::Normalize()
{
    float mag = GetMagnitude();
    float invMag = 1.0f / mag;
    if (0.0f * invMag == 0.0f * invMag)
    {
        X *= invMag;
        Y *= invMag;
        Z *= invMag;
    }
    return *this;
}

float Vec3::GetMagnitude() const
{
    float mag;
    mag = X * X + Y * Y + Z * Z;
    mag = sqrtf(mag);
    return mag;
}

bool Vec3::IsValid() const
{
    if (X * 0.0f != X * 0.0f)
    {
        return false;
    }
    if (Y * 0.0f != Y * 0.0f)
    {
        return false;
    }
    if (Z * 0.0f != Z * 0.0f)
    {
        return false;
    }
    return true;
}

std::string Vec3::ToString() const
{
    return "{" + std::to_string(X) + "," + std::to_string(Y) + "," + std::to_string(Z) + "}";
}
