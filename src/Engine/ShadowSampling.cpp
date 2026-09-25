#include "ShadowSampling.h"

#include "stdafx.h"

namespace ShadowSampling
{
    SIMDFloat Visibility(const SIMDPixel& pixel, const DepthBuffer& depthBuffer, DirectionalLight& light)
    {
        SIMDFloat visible = SIMD::ONE;
        for (int i = 0; i < SIMDPixel::PIXEL_WIDTH * SIMDPixel::PIXEL_HEIGHT; ++i)
        {
            Vec3 position(pixel.WorldSpacePosition.X.V[i],
                          pixel.WorldSpacePosition.Y.V[i],
                          pixel.WorldSpacePosition.Z.V[i]);
            Vec3 lightSpace = light.WorldToLightSpace(position);
            Vec4 projected = light.Proj * Vec4(lightSpace);
            // Behind the light
            if (projected.W <= 0.0f)
                continue;
            float inverseW = 1.0f / projected.W;
            Vec4 ndc = projected * inverseW;
            // Outside the light's view: nothing was drawn there, lit
            if (ndc.X < -1.0f || ndc.X > 1.0f || ndc.Y < -1.0f || ndc.Y > 1.0f || ndc.Z < -1.0f ||
                ndc.Z > 1.0f)
                continue;
            depthBuffer.ToShadowSpace(ndc);
            float closest = depthBuffer.GetBufferSingle(static_cast<int>(ndc.X), static_cast<int>(ndc.Y), true);
            if (closest > inverseW * (1.0f + BIAS))
                visible.V[i] = 0.0f;
        }
        return visible;
    }
} // namespace ShadowSampling
