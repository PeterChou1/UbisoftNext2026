#include "Quat.h"

#include "stdafx.h"

#include <cassert>
#include <cmath>
#include <math.h>

Quat::Quat()
    : X(0)
    , Y(0)
    , Z(0)
    , W(1)
{
}

Quat::Quat(const Quat& rhs)
    : X(rhs.X)
    , Y(rhs.Y)
    , Z(rhs.Z)
    , W(rhs.W)
{
}

Quat::Quat(float X, float Y, float Z, float W)
    : X(X)
    , Y(Y)
    , Z(Z)
    , W(W)
{
}

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

const Quat& Quat::operator=(const Quat& rhs)
{
    X = rhs.X;
    Y = rhs.Y;
    Z = rhs.Z;
    W = rhs.W;
    return *this;
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
    Quat temp = *this * rhs;
    W = temp.W;
    X = temp.X;
    Y = temp.Y;
    Z = temp.Z;
    return *this;
}

Quat Quat::operator*(const Quat& rhs) const
{
    Quat temp;
    temp.W = W * rhs.W - X * rhs.X - Y * rhs.Y - Z * rhs.Z;
    temp.X = X * rhs.W + W * rhs.X + Y * rhs.Z - Z * rhs.Y;
    temp.Y = Y * rhs.W + W * rhs.Y + Z * rhs.X - X * rhs.Z;
    temp.Z = Z * rhs.W + W * rhs.Z + X * rhs.Y - Y * rhs.X;
    return temp;
}

void Quat::Normalize()
{
    float magnitude = GetMagnitude();
    assert(magnitude != 0.0);
    float invMag = 1.0f / magnitude;
    X = X * invMag;
    Y = Y * invMag;
    Z = Z * invMag;
    W = W * invMag;
}

void Quat::Invert()
{
    *this *= 1.0f / MagnitudeSquared();
    X = -X;
    Y = -Y;
    Z = -Z;
}

Quat Quat::Inverse() const
{
    Quat val(*this);
    val.Invert();
    return val;
}

float Quat::MagnitudeSquared() const
{
    return X * X + Y * Y + Z * Z + W * W;
}

float Quat::GetMagnitude() const
{
    return sqrtf(MagnitudeSquared());
}

Vec3 Quat::RotatePoint(const Vec3& rhs) const
{
    Quat vector(rhs.X, rhs.Y, rhs.Z, 0.0f);
    Quat finalQuat = *this * vector * Inverse();
    return Vec3(finalQuat.X, finalQuat.Y, finalQuat.Z);
}

float Quat::GetRoll2D() const
{
    float angle = 2.0f * std::atan2(X, W);
    return angle;
}

float Quat::GetYaw2D() const
{
    float angle = 2.0f * std::atan2(Z, W);
    return angle;
}

float Quat::GetPitch2D() const
{
    float angle = 2.0f * std::atan2(Y, W);
    return angle;
}

Quat Quat::FromRotationMatrix(Mat3& m)
{
    float trace = m[0][0] + m[1][1] + m[2][2];
    Quat q;

    if (trace > 0)
    {
        float S = std::sqrtf(trace + 1.0f) * 2.0f; // S=4*qw
        q.W = 0.25f * S;
        q.X = (m[2][1] - m[1][2]) / S;
        q.Y = (m[0][2] - m[2][0]) / S;
        q.Z = (m[1][0] - m[0][1]) / S;
    }
    else if ((m[0][0] > m[1][1]) & (m[0][0] > m[2][2]))
    {
        float S = std::sqrtf(1.0f + m[0][0] - m[1][1] - m[2][2]) * 2.0f; // S=4*qx
        q.W = (m[2][1] - m[1][2]) / S;
        q.X = 0.25f * S;
        q.Y = (m[0][1] + m[1][0]) / S;
        q.Z = (m[0][2] + m[2][0]) / S;
    }
    else if (m[1][1] > m[2][2])
    {
        float S = std::sqrtf(1.0f + m[1][1] - m[0][0] - m[2][2]) * 2.0f; // S=4*qy
        q.W = (m[0][2] - m[2][0]) / S;
        q.X = (m[0][1] + m[1][0]) / S;
        q.Y = 0.25f * S;
        q.Z = (m[1][2] + m[2][1]) / S;
    }
    else
    {
        float S = std::sqrtf(1.0f + m[2][2] - m[0][0] - m[1][1]) * 2.0f; // S=4*qz
        q.W = (m[1][0] - m[0][1]) / S;
        q.X = (m[0][2] + m[2][0]) / S;
        q.Y = (m[1][2] + m[2][1]) / S;
        q.Z = 0.25f * S;
    }
    return q;
}
