//---------------------------------------------------------------------------------
// BlingPhong.h
//---------------------------------------------------------------------------------
//
// A Blinn Phong Shader see
// https://en.wikipedia.org/wiki/Blinn%E2%80%93Phong_reflection_model
// for more information
//
#pragma once
#include "FragmentShader.h"

class BlinnPhongSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& light) override;
};
