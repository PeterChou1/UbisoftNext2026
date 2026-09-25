//---------------------------------------------------------------------------------
// Quat.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a Quaternion
//
#pragma once
#include "Mat3.h"
#include "Vec3.h"

class Quat
{
  public:
    /// Identity rotation
    Quat()
        : W(1)
        , X(0)
        , Y(0)
        , Z(0)
    {
    }

    Quat(float X, float Y, float Z, float W)
        : W(W)
        , X(X)
        , Y(Y)
        , Z(Z)
    {
    }

    /// Rotation of angleRadians around the axis n (normalized here)
    Quat(Vec3 n, float angleRadians);

    Quat& operator*=(const float& rhs);
    Quat& operator*=(const Quat& rhs);
    Quat operator*(const Quat& rhs) const;
    void Normalize();
    Quat Inverse() const;
    float MagnitudeSquared() const { return X * X + Y * Y + Z * Z + W * W; }
    float GetMagnitude() const;
    Vec3 RotatePoint(const Vec3& rhs) const;

    /// Angle of a rotation around a single axis (X, Z and Y respectively)
    float GetRoll2D() const;
    float GetYaw2D() const;
    float GetPitch2D() const;

    static Quat FromRotationMatrix(Mat3& m);

    float W;
    float X;
    float Y;
    float Z;
};
