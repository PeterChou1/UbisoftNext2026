#include "ShaderSIMDUtils.h"

#include "stdafx.h"

SIMDVec3 MixColor(SIMDVec3& colorA, SIMDVec3& colorB)
{
    SIMDVec3 mixColor;
    mixColor.X = (colorA.X + colorB.X) / 2;
    mixColor.Y = (colorA.Y + colorB.Y) / 2;
    mixColor.Z = (colorA.Z + colorB.Z) / 2;
    return mixColor;
}
