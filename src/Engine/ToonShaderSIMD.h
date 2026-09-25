//---------------------------------------------------------------------------------
// ToonShaderSIMD.h
//---------------------------------------------------------------------------------
//
// Placeholder for a toon shader: it leaves the pixels' colour untouched
//
#pragma once
#include "FragmentShader.h"

class ToonShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel&, DepthBuffer&, Material&, Camera&, DirectionalLight&) override {}
};
