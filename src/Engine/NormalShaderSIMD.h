//---------------------------------------------------------------------------------
// NormalShaderSIMD.h
//---------------------------------------------------------------------------------
//
// Debug shader that outputs the normal
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
               DirectionalLight& light) override;
};
