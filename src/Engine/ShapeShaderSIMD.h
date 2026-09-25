//---------------------------------------------------------------------------------
// ShapeShaderSIMD.h
//---------------------------------------------------------------------------------
//
// Lit shader for procedural shapes: the surface colour comes from the vertex
// colour (set per shape) instead of an .obj material, lit by the scene's
// directional light with an ambient term
//
#pragma once
#include "FragmentShader.h"

class ShapeShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& light) override;
};
