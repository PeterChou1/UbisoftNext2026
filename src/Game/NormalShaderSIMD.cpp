#include "NormalShaderSIMD.h"

#include "stdafx.h"

void NormalShaderSIMD::Shade(SIMDPixel& pixel,
                             DepthBuffer& depthBuffer,
                             Material& texture,
                             Camera& cam,
                             DirectionalLight& Light)
{
    SIMDVec3 normal = pixel.Normal.Normalize();
    pixel.Color.X = normal.X;
    pixel.Color.Y = normal.Y;
    pixel.Color.Z = normal.Z;
}
