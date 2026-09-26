#include "ParticleShaderSIMD.h"

#include "stdafx.h"

void ParticleShaderSIMD::Shade(
        SIMDPixel& pixel, DepthBuffer&, Material&, Camera&, DirectionalLight&)
{
    pixel.Color = pixel.VertexColor;
}
