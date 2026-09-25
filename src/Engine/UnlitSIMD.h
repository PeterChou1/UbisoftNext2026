//---------------------------------------------------------------------------------
// UnlitSIMD.h
//---------------------------------------------------------------------------------
//
// Basic Unlit Shader shades the color based off the texture
// removes all lighting calculation
//
#pragma once
#include "FragmentShader.h"

class UnlitSIMD : public FragmentShader
{
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& light) override;
};
