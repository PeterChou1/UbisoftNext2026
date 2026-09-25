#pragma once
#include "BlingPhong.h"

// Blinn-Phong, black near the triangle's edges
class OutlineScanShaderSIMD : public BlinnPhongSIMD
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& light) override;
};
