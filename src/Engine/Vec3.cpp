#include "Vec3.h"

#include <cmath>

const Vec3& Vec3::operator+=(const Vec3& rhs)
{
    X += rhs.X;
    Y += rhs.Y;
    Z += rhs.Z;
    return *this;
}

const Vec3& Vec3::operator*=(float rhs)
{
    X *= rhs;
    Y *= rhs;
    Z *= rhs;
    return *this;
}

Vec3 Vec3::Cross(const Vec3& rhs) const
{
    return Vec3((Y * rhs.Z) - (rhs.Y * Z), (rhs.X * Z) - (X * rhs.Z), (X * rhs.Y) - (rhs.X * Y));
}

Vec3& Vec3::Normalize()
{
    float invMag = 1.0f / GetMagnitude();
    // Skip when invMag is infinite (zero length) or NaN
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
    return std::sqrt(X * X + Y * Y + Z * Z);
}

bool Vec3::IsValid() const
{
    // x * 0 is NaN exactly when x is NaN or infinite
    return X * 0.0f == X * 0.0f && Y * 0.0f == Y * 0.0f && Z * 0.0f == Z * 0.0f;
}

std::string Vec3::ToString() const
{
    return "{" + std::to_string(X) + "," + std::to_string(Y) + "," + std::to_string(Z) + "}";
}
