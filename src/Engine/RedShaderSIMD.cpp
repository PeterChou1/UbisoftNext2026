#include "RedShaderSIMD.h"

#include "stdafx.h"

void RedShaderSIMD::Shade(SIMDPixel& pixel, DepthBuffer&, Material&, Camera&, DirectionalLight&)
{
    pixel.Color = Vec3(1.0, 0.0, 0.0);
}
