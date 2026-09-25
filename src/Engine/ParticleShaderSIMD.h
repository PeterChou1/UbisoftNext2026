#pragma once
#include "FragmentShader.h"

// Particles take their vertex colour, unlit
class ParticleShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& light) override;
};
