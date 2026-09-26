//---------------------------------------------------------------------------------
// Mat2.h
//---------------------------------------------------------------------------------
//
// 2x2 matrix (2D rotations)
//
#pragma once
#include "Vec2.h"

class Mat2
{
  public:
    Mat2() {}
    Mat2(const Vec2& row0, const Vec2& row1) : Rows{row0, row1} {}

    Vec2 operator*(const Vec2& rhs) const { return Vec2(Rows[0].Dot(rhs), Rows[1].Dot(rhs)); }

    Vec2 Rows[2];
};
