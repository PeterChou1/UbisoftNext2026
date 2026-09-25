#include "UnlitSIMD.h"

#include "stdafx.h"

void UnlitSIMD::Shade(SIMDPixel& pixel, DepthBuffer&, Material& texture, Camera&, DirectionalLight&)
{
    SIMDFloat r, g, b;
    texture.SampleSIMD(r, g, b);
    pixel.Color = SIMDVec3(r / 255.0, g / 255.0, b / 255.0);
}
