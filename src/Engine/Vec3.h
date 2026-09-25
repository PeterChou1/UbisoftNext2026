//---------------------------------------------------------------------------------
// Vec3.h
//---------------------------------------------------------------------------------
//
// Basic Implementation of a 3d Vector
//
#pragma once
#include <string>

class Vec3
{
  public:
    Vec3();
    Vec3(const Vec3& rhs);
    Vec3(float X, float Y, float Z);

    Vec3& operator=(const Vec3& rhs);

    bool operator==(const Vec3& rhs) const;

    Vec3 operator+(const Vec3& rhs) const;
    const Vec3& operator+=(const Vec3& rhs);

    Vec3 operator-(const Vec3& rhs) const;
    Vec3 operator*(const float rhs) const;
    Vec3 operator/(const float rhs) const;

    const Vec3& operator*=(const float rhs);

    float operator[](const int idx) const;
    float& operator[](const int idx);

    void Zero()
    {
        X = 0.0f;
        Y = 0.0f;
        Z = 0.0f;
    }

    Vec3 Cross(const Vec3& rhs) const;
    float Dot(const Vec3& rhs) const;
    Vec3& Normalize();
    float GetMagnitude() const;

    float GetLengthSqr() const { return Dot(*this); }

    bool IsValid() const;

    const float* ToPtr() const { return &X; }

    std::string ToString() const;

    float X;
    float Y;
    float Z;
};
