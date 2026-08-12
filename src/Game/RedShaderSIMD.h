//---------------------------------------------------------------------------------
// RedShader.h
//---------------------------------------------------------------------------------
//
//  A shader that only outputs red
//

#pragma once
#include "FragmentShader.h"

class RedShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;

    ~RedShaderSIMD() override = default;
};