//---------------------------------------------------------------------------------
// Mat3.h
//---------------------------------------------------------------------------------
//
// 3x3 matrix (rotations and the minors of Mat4)
//
#pragma once
#include "Vec3.h"

class Quat;

class Mat3
{
  public:
    float Determinant() const;
    Vec3& operator[](int i);

    /// Rotation matrix of a quaternion
    static Mat3 FromQuat(const Quat& q);

    Vec3 Rows[3];
};
