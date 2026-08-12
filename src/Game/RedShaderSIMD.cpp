#include "RedShaderSIMD.h"

#include "stdafx.h"

void RedShaderSIMD::Shade(SIMDPixel& pixel,
                          DepthBuffer& depthBuffer,
                          Material& texture,
                          Camera& cam,
                          DirectionalLight& Light)
{
    pixel.Color = Vec3(1.0, 0.0, 0.0);
}
