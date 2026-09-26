#include "ShadowSampling.h"

#include "stdafx.h"

#include <algorithm>
#include <cmath>

namespace ShadowSampling
{
    float Visibility(const Vec3& position,
                     const Vec3& normal,
                     const DepthBuffer& depthBuffer,
                     DirectionalLight& light)
    {
        const bool parallel = light.lightType == ParallelLight;
        // One texel of the shadow map, in world units, at this point
        float texel = light.TexelSize;
        if (!parallel)
        {
            Vec3 toPoint = position - light.Position;
            texel *= std::sqrt(toPoint.Dot(toPoint));
        }
        Vec3 n = normal;
        if (n.Dot(n) > 1e-12f)
            n.Normalize();
        Vec3 lookup = position + n * (NORMAL_OFFSET * texel);
        // Slope: where the light grazes a surface, one texel spans a long
        // stretch of it, so the depth needs more room (tangent of the angle
        // between the normal and the light, capped)
        Vec3 toLight = light.Direction * -1.0f;
        if (!parallel)
        {
            toLight = light.Position - position;
            if (toLight.Dot(toLight) > 1e-12f)
                toLight.Normalize();
        }
        float cosine = std::clamp(std::fabs(n.Dot(toLight)), 0.05f, 1.0f);
        float slope = std::min(std::sqrt(1.0f - cosine * cosine) / cosine, MAX_SLOPE);
        float slopeBias = SLOPE_BIAS * slope * texel; // world units

        Vec3 lightSpace = light.WorldToLightSpace(lookup);
        Vec4 projected = light.Proj * Vec4(lightSpace);
        float own = 0.0f;
        Vec4 ndc = projected;
        if (parallel)
        {
            own = (1.0f - projected.Z) * 0.5f;
        }
        else
        {
            // Behind the light
            if (projected.W <= 1e-6f)
                return 1.0f;
            own = 1.0f / projected.W;
            ndc = projected * own;
        }
        // Outside the light's view: nothing was drawn there
        if (ndc.X < -1.0f || ndc.X > 1.0f || ndc.Y < -1.0f || ndc.Y > 1.0f || ndc.Z < -1.0f ||
            ndc.Z > 1.0f)
            return 1.0f;

        // Shadow map texels sample integer positions (like the rasterizer)
        const int width = depthBuffer.ShadowTexelsX();
        const int height = depthBuffer.ShadowTexelsY();
        float u = (ndc.X + 1.0f) * 0.5f * static_cast<float>(width);
        float v = (ndc.Y + 1.0f) * 0.5f * static_cast<float>(height);
        float fu = std::floor(u);
        float fv = std::floor(v);
        int x0 = static_cast<int>(fu);
        int y0 = static_cast<int>(fv);
        float tx = u - fu;
        float ty = v - fv;

        // Something closer to the light than this point at texel (x, y)?
        const float threshold =
                parallel ? own + (PARALLEL_BIAS + slopeBias) * light.DepthPerUnit
                         : own * (1.0f + SPOT_BIAS + slopeBias / std::max(1.0f / own, 1e-3f));
        auto shadowed = [&](int x, int y) {
            // Off the map: nothing there
            if (x < 0 || y < 0 || x >= width || y >= height)
                return 0.0f;
            return depthBuffer.ShadowDepth(x, y) > threshold ? 1.0f : 0.0f;
        };
        float s00 = shadowed(x0, y0);
        float s10 = shadowed(x0 + 1, y0);
        float s01 = shadowed(x0, y0 + 1);
        float s11 = shadowed(x0 + 1, y0 + 1);
        float shadow =
                (s00 * (1.0f - tx) + s10 * tx) * (1.0f - ty) + (s01 * (1.0f - tx) + s11 * tx) * ty;
        return 1.0f - shadow;
    }

    SIMDFloat
    Visibility(const SIMDPixel& pixel, const DepthBuffer& depthBuffer, DirectionalLight& light)
    {
        SIMDFloat visible = SIMD::ONE;
        for (int i = 0; i < SIMDPixel::PIXEL_WIDTH * SIMDPixel::PIXEL_HEIGHT; ++i)
        {
            Vec3 position(pixel.WorldSpacePosition.X.V[i],
                          pixel.WorldSpacePosition.Y.V[i],
                          pixel.WorldSpacePosition.Z.V[i]);
            Vec3 normal(pixel.Normal.X.V[i], pixel.Normal.Y.V[i], pixel.Normal.Z.V[i]);
            visible.V[i] = Visibility(position, normal, depthBuffer, light);
        }
        return visible;
    }
} // namespace ShadowSampling
