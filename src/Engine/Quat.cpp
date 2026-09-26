#include "Quat.h"

#include <cassert>
#include <cmath>

Quat::Quat(Vec3 n, const float angleRadians)
{
    const float halfAngleRadians = 0.5f * angleRadians;
    W = cosf(halfAngleRadians);
    const float halfSine = sinf(halfAngleRadians);
    n.Normalize();
    X = n.X * halfSine;
    Y = n.Y * halfSine;
    Z = n.Z * halfSine;
}

Quat& Quat::operator*=(const float& rhs)
{
    X *= rhs;
    Y *= rhs;
    Z *= rhs;
    W *= rhs;
    return *this;
}

Quat& Quat::operator*=(const Quat& rhs)
{
    *this = *this * rhs;
    return *this;
}

Quat Quat::operator*(const Quat& rhs) const
{
    Quat product;
    product.W = W * rhs.W - X * rhs.X - Y * rhs.Y - Z * rhs.Z;
    product.X = X * rhs.W + W * rhs.X + Y * rhs.Z - Z * rhs.Y;
    product.Y = Y * rhs.W + W * rhs.Y + Z * rhs.X - X * rhs.Z;
    product.Z = Z * rhs.W + W * rhs.Z + X * rhs.Y - Y * rhs.X;
    return product;
}

void Quat::Normalize()
{
    float magnitude = GetMagnitude();
    assert(magnitude != 0.0);
    *this *= 1.0f / magnitude;
}

Quat Quat::Inverse() const
{
    // Conjugate divided by the squared magnitude
    Quat inverse = *this;
    inverse *= 1.0f / MagnitudeSquared();
    inverse.X = -inverse.X;
    inverse.Y = -inverse.Y;
    inverse.Z = -inverse.Z;
    return inverse;
}

float Quat::GetMagnitude() const
{
    return std::sqrt(MagnitudeSquared());
}

Vec3 Quat::RotatePoint(const Vec3& rhs) const
{
    Quat rotated = *this * Quat(rhs.X, rhs.Y, rhs.Z, 0.0f) * Inverse();
    return Vec3(rotated.X, rotated.Y, rotated.Z);
}

float Quat::GetRoll2D() const
{
    return 2.0f * std::atan2(X, W);
}

float Quat::GetYaw2D() const
{
    return 2.0f * std::atan2(Z, W);
}

float Quat::GetPitch2D() const
{
    return 2.0f * std::atan2(Y, W);
}

Quat Quat::FromRotationMatrix(Mat3& m)
{
    float trace = m[0][0] + m[1][1] + m[2][2];
    Quat q;

    if (trace > 0)
    {
        float S = std::sqrt(trace + 1.0f) * 2.0f; // S=4*qw
        q.W = 0.25f * S;
        q.X = (m[2][1] - m[1][2]) / S;
        q.Y = (m[0][2] - m[2][0]) / S;
        q.Z = (m[1][0] - m[0][1]) / S;
    }
    else if ((m[0][0] > m[1][1]) & (m[0][0] > m[2][2]))
    {
        float S = std::sqrt(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f; // S=4*qx
        q.W = (m[2][1] - m[1][2]) / S;
        q.X = 0.25f * S;
        q.Y = (m[0][1] + m[1][0]) / S;
        q.Z = (m[0][2] + m[2][0]) / S;
    }
    else if (m[1][1] > m[2][2])
    {
        float S = std::sqrt(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f; // S=4*qy
        q.W = (m[0][2] - m[2][0]) / S;
        q.X = (m[0][1] + m[1][0]) / S;
        q.Y = 0.25f * S;
        q.Z = (m[1][2] + m[2][1]) / S;
    }
    else
    {
        float S = std::sqrt(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f; // S=4*qz
        q.W = (m[1][0] - m[0][1]) / S;
        q.X = (m[0][2] + m[2][0]) / S;
        q.Y = (m[1][2] + m[2][1]) / S;
        q.Z = 0.25f * S;
    }
    return q;
}
