#include "ParticleShaderSIMD.h"

#include "stdafx.h"

void ParticleShaderSIMD::Shade(SIMDPixel& pixel,
                               DepthBuffer& depthBuffer,
                               Material& texture,
                               Camera& camera,
                               DirectionalLight& Light)
{
    pixel.Color = pixel.VertexColor;
}
