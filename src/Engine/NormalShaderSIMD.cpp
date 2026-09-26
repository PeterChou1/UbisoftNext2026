#include "NormalShaderSIMD.h"

#include "stdafx.h"

void NormalShaderSIMD::Shade(SIMDPixel& pixel, DepthBuffer&, Material&, Camera&, DirectionalLight&)
{
    pixel.Color = pixel.Normal.Normalize();
}
