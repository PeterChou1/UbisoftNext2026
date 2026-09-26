#include "OutlineShaderSIMD.h"

#include "stdafx.h"

void OutlineScanShaderSIMD::Shade(SIMDPixel& pixel,
                                  DepthBuffer& depthBuffer,
                                  Material& texture,
                                  Camera& camera,
                                  DirectionalLight& light)
{
    BlinnPhongSIMD::Shade(pixel, depthBuffer, texture, camera, light);
    const SIMDFloat nearEdgeA = pixel.Alpha < SIMD::EPSILON;
    const SIMDFloat nearEdgeB = pixel.Beta < SIMD::EPSILON;
    const SIMDFloat nearEdgeC = pixel.Gamma < SIMD::EPSILON;
    for (SIMDFloat* channel : {&pixel.Color.X, &pixel.Color.Y, &pixel.Color.Z})
    {
        *channel = SIMD::Select(nearEdgeA, *channel, SIMD::ZERO);
        *channel = SIMD::Select(nearEdgeB, *channel, SIMD::ZERO);
        *channel = SIMD::Select(nearEdgeC, *channel, SIMD::ZERO);
    }
}
