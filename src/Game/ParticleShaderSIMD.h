#pragma once
#include "FragmentShader.h"

class ParticleShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;

    ~ParticleShaderSIMD() override = default;
};
