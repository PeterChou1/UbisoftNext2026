#include "SIMDTriangle.h"

#include "stdafx.h"

SIMDTriangle::SIMDTriangle(const Triangle& t)
    : B0(SIMDFloat(static_cast<float>(t.B0)))
    , C0(SIMDFloat(static_cast<float>(t.C0)))
    , B1(SIMDFloat(static_cast<float>(t.B1)))
    , C1(SIMDFloat(static_cast<float>(t.C1)))
    , B2(SIMDFloat(static_cast<float>(t.B2)))
    , C2(SIMDFloat(static_cast<float>(t.C2)))
    , InvDet(t.invDet)
    , V1(SIMDVec2(t.verts[0].Projection.X, t.verts[0].Projection.Y))
    , V2(SIMDVec2(t.verts[1].Projection.X, t.verts[1].Projection.Y))
    , V3(SIMDVec2(t.verts[2].Projection.X, t.verts[2].Projection.Y))
    , InvW1(t.verts[0].InverseW)
    , InvW2(t.verts[1].InverseW)
    , InvW3(t.verts[2].InverseW)
{
}
