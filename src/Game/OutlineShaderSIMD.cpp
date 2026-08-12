#include "OutlineShaderSIMD.h"

#include "stdafx.h"

void OutlineScanShaderSIMD::Shade(SIMDPixel& pixel,
                                  DepthBuffer& depthBuffer,
                                  Material& texture,
                                  Camera& camera,
                                  DirectionalLight& Light)
{
    BlinnPhongSIMD::Shade(pixel, depthBuffer, texture, camera, Light);
    SIMDFloat AlphaMask = pixel.Alpha < SIMD::EPSILON;
    SIMDFloat BetaMask = pixel.Beta < SIMD::EPSILON;
    SIMDFloat GammaMask = pixel.Gamma < SIMD::EPSILON;

    pixel.Color.X = SIMD::Select(AlphaMask, pixel.Color.X, SIMD::ZERO);
    pixel.Color.X = SIMD::Select(BetaMask, pixel.Color.X, SIMD::ZERO);
    pixel.Color.X = SIMD::Select(GammaMask, pixel.Color.X, SIMD::ZERO);

    pixel.Color.Y = SIMD::Select(AlphaMask, pixel.Color.Y, SIMD::ZERO);
    pixel.Color.Y = SIMD::Select(BetaMask, pixel.Color.Y, SIMD::ZERO);
    pixel.Color.Y = SIMD::Select(GammaMask, pixel.Color.Y, SIMD::ZERO);

    pixel.Color.Z = SIMD::Select(AlphaMask, pixel.Color.Z, SIMD::ZERO);
    pixel.Color.Z = SIMD::Select(BetaMask, pixel.Color.Z, SIMD::ZERO);
    pixel.Color.Z = SIMD::Select(GammaMask, pixel.Color.Z, SIMD::ZERO);
}
