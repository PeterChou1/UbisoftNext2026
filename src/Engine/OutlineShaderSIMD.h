#pragma once
#include "BlingPhong.h"

class OutlineScanShaderSIMD : public BlinnPhongSIMD
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;

    ~OutlineScanShaderSIMD() override = default;
};
