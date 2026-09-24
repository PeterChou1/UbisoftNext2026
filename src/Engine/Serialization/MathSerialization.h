//---------------------------------------------------------------------------------
// MathSerialization.h
//---------------------------------------------------------------------------------
//
// Serialize functions for the engine math types. Values are stored bit exact
// (IEEE-754) so a save / load round trip never changes a single float
//
#pragma once

#include "../Mat2.h"
#include "../Mat3.h"
#include "../Mat4.h"
#include "../Quat.h"
#include "../Vec2.h"
#include "../Vec3.h"
#include "../Vec4.h"
#include "Archive.h"

template <typename Archive>
void Serialize(Archive& ar, Vec2& v)
{
    ar(v.X, v.Y);
}

template <typename Archive>
void Serialize(Archive& ar, Vec3& v)
{
    ar(v.X, v.Y, v.Z);
}

template <typename Archive>
void Serialize(Archive& ar, Vec4& v)
{
    ar(v.X, v.Y, v.Z, v.W);
}

template <typename Archive>
void Serialize(Archive& ar, Quat& q)
{
    ar(q.W, q.X, q.Y, q.Z);
}

template <typename Archive>
void Serialize(Archive& ar, Mat2& m)
{
    ar(m.Rows[0], m.Rows[1]);
}

template <typename Archive>
void Serialize(Archive& ar, Mat3& m)
{
    ar(m.Rows[0], m.Rows[1], m.Rows[2]);
}

template <typename Archive>
void Serialize(Archive& ar, Mat4& m)
{
    ar(m.Rows[0], m.Rows[1], m.Rows[2], m.Rows[3]);
}
