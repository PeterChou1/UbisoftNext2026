//---------------------------------------------------------------------------------
// RedShader.h
//---------------------------------------------------------------------------------
//
// Debug shader that output normal
//
#pragma once
#include "FragmentShader.h"

class NormalShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;

    ~NormalShaderSIMD() override = default;
};