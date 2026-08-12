//---------------------------------------------------------------------------------
// ToonShaderSIMD.h
//---------------------------------------------------------------------------------
//
// Basic Toon Shader based off of this tutorial
// https://www.lighthouse3d.com/tutorials/glsl-12-tutorial/toon-shader-version-ii/
//
#pragma once
#include "FragmentShader.h"

class ToonShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;

    ~ToonShaderSIMD() override = default;
};
