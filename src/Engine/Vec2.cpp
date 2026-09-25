#include "Vec2.h"

#include <cmath>

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

const Vec2& Vec2::operator*=(float rhs)
{
    X *= rhs;
    Y *= rhs;
    return *this;
}

const Vec2& Vec2::Normalize()
{
    float invMag = 1.0f / GetMagnitude();
    // Skip when invMag is infinite (zero length) or NaN
    if (0.0f * invMag == 0.0f * invMag)
    {
        X *= invMag;
        Y *= invMag;
    }
    return *this;
}

float Vec2::GetMagnitude() const
{
    return std::sqrt(X * X + Y * Y);
}

std::string Vec2::ToString() const
{
    return "{" + std::to_string(X) + "," + std::to_string(Y) + "}";
}
