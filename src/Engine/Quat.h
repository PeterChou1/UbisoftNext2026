//---------------------------------------------------------------------------------
// Quat.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a Quaternion
//
#pragma once
#include "Mat3.h"
#include "Vec3.h"
#include "Vec4.h"

class Quat
{
  public:
    Quat();
    Quat(const Quat& rhs);
    Quat(float X, float Y, float Z, float W);
    Quat(Vec3 n, float angleRadians);
    const Quat& operator=(const Quat& rhs);
    Quat& operator*=(const float& rhs);
    Quat& operator*=(const Quat& rhs);
    Quat operator*(const Quat& rhs) const;
    void Normalize();
    void Invert();
    Quat Inverse() const;
    float MagnitudeSquared() const;
    float GetMagnitude() const;
    Vec3 RotatePoint(const Vec3& rhs) const;

    float GetRoll2D() const;
    float GetYaw2D() const;
    float GetPitch2D() const;

    static Quat FromRotationMatrix(Mat3& m);
    float W;
    float X;
    float Y;
    float Z;
};
