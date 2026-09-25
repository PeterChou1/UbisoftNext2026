#include "EffectShadersSIMD.h"

#include "ShadowSampling.h"
#include "stdafx.h"

#include <algorithm>
#include <cmath>

namespace
{
    SIMDVec3 Saturate(const SIMDVec3& color)
    {
        return {SIMD::Min(color.X, SIMD::ONE),
                SIMD::Min(color.Y, SIMD::ONE),
                SIMD::Min(color.Z, SIMD::ONE)};
    }

    // The surface colour lit by the scene's light (as the Shape shader does)
    SIMDVec3 Lit(const SIMDVec3& base,
                 SIMDPixel& pixel,
                 DirectionalLight& light,
                 const DepthBuffer& depthBuffer,
                 bool shadows)
    {
        return base * EffectShading::Lighting(pixel, light, depthBuffer, shadows) *
               EffectShading::LightColor(light);
    }
} // namespace

namespace EffectShading
{
    SIMDVec3 BaseColor(const SIMDPixel& pixel, const Material& material)
    {
        SIMDFloat r, g, b;
        material.SampleSIMD(r, g, b);
        const SIMDVec3& vertex = pixel.VertexColor;
        SIMDFloat black = (vertex.X + vertex.Y + vertex.Z) <= SIMD::ZERO;
        return {SIMD::Select(black, vertex.X, r / 255.0f),
                SIMD::Select(black, vertex.Y, g / 255.0f),
                SIMD::Select(black, vertex.Z, b / 255.0f)};
    }

    SIMDFloat Lighting(SIMDPixel& pixel,
                       DirectionalLight& light,
                       const DepthBuffer& depthBuffer,
                       bool shadows)
    {
        const SIMDFloat ambient = light.Ambient;
        SIMDVec3 normal = pixel.Normal.Normalize();
        SIMDFloat cone;
        SIMDVec3 lightDir = ToLight(pixel, light, cone);
        SIMDFloat diffuse =
                SIMD::Max(normal.Dot(lightDir), SIMD::ZERO) * cone * (0.65f * light.Intensity);
        if (shadows)
            diffuse = diffuse * ShadowSampling::Visibility(pixel, depthBuffer, light);
        return SIMD::Clamp(SIMD::ZERO, SIMD::ONE, ambient + diffuse);
    }

    SIMDVec3 ToLight(const SIMDPixel& pixel, const DirectionalLight& light, SIMDFloat& cone)
    {
        if (light.lightType == ParallelLight)
        {
            cone = SIMD::ONE;
            return SIMDVec3(-light.Direction.X, -light.Direction.Y, -light.Direction.Z);
        }
        SIMDVec3 lightPos = light.Position;
        SIMDVec3 toLight = (lightPos - pixel.WorldSpacePosition).Normalize();
        if (light.SpotCosOuter <= -1.0f)
        {
            cone = SIMD::ONE;
            return toLight;
        }
        // Angle from the spot's axis: full light inside, none past the edge
        SIMDVec3 axis(light.Direction.X, light.Direction.Y, light.Direction.Z);
        SIMDFloat cosAngle = (toLight * SIMDFloat(-1.0f)).Dot(axis);
        float range = std::max(light.SpotCosInner - light.SpotCosOuter, 1e-4f);
        cone = SIMD::Clamp(SIMD::ZERO,
                           SIMD::ONE,
                           (cosAngle - SIMDFloat(light.SpotCosOuter)) / SIMDFloat(range));
        return toLight;
    }

    SIMDVec3 LightColor(const DirectionalLight& light)
    {
        return SIMDVec3(light.Color.X, light.Color.Y, light.Color.Z);
    }
} // namespace EffectShading

void PulseShaderSIMD::Shade(SIMDPixel& pixel,
                            DepthBuffer& depthBuffer,
                            Material& texture,
                            Camera&,
                            DirectionalLight& light)
{
    // One value per draw: the whole object pulses together
    const float wave = 0.5f + 0.5f * std::sin(DeltaTime * SPEED);
    SIMDVec3 lit =
            Lit(EffectShading::BaseColor(pixel, texture), pixel, light, depthBuffer, ShadowMapping);
    // From 55% of the lit colour up to all of it plus a white glow: dimming
    // (not only brightening) keeps the pulse visible on bright surfaces,
    // which would otherwise stay clipped at white
    SIMDFloat scale = 0.55f + 0.45f * wave;
    SIMDFloat white = 0.15f * wave;
    pixel.Color = Saturate(lit * scale + SIMDVec3(white, white, white));
}

void RimShaderSIMD::Shade(SIMDPixel& pixel,
                          DepthBuffer& depthBuffer,
                          Material& texture,
                          Camera& camera,
                          DirectionalLight& light)
{
    SIMDVec3 base = EffectShading::BaseColor(pixel, texture);
    SIMDVec3 lit = Lit(base, pixel, light, depthBuffer, ShadowMapping);
    SIMDVec3 camPos = camera.Position;
    SIMDVec3 normal = pixel.Normal.Normalize();
    SIMDVec3 viewDir = (camPos - pixel.WorldSpacePosition).Normalize();
    // 0 facing the camera, 1 at grazing angles; cubed to keep it on the edges
    SIMDFloat facing = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, SIMD::Abs(normal.Dot(viewDir)));
    SIMDFloat rim = SIMD::ONE - facing;
    rim = rim * rim * rim;
    // The rim is a light tint of the base colour
    SIMDVec3 glow = base * SIMDFloat(0.5f) + SIMDVec3(0.5f, 0.5f, 0.5f);
    // The body is kept a little below full brightness so the rim still shows
    // on bright surfaces
    pixel.Color = Saturate(lit * SIMDFloat(0.85f) + glow * rim);
}

void StripesShaderSIMD::Shade(SIMDPixel& pixel,
                              DepthBuffer& depthBuffer,
                              Material& texture,
                              Camera&,
                              DirectionalLight& light)
{
    SIMDVec3 lit =
            Lit(EffectShading::BaseColor(pixel, texture), pixel, light, depthBuffer, ShadowMapping);
    // Position in the band pattern, 0..1 inside each band
    SIMDFloat band = pixel.WorldSpacePosition.Y * DENSITY - SIMDFloat(DeltaTime * SPEED * DENSITY);
    SIMDFloat fraction = band - band.floor();
    // Bright half and dim half of every band
    SIMDFloat bright = fraction < SIMDFloat(0.5f);
    SIMDFloat scale = SIMD::Select(bright, SIMDFloat(0.45f), SIMDFloat(1.15f));
    pixel.Color = Saturate(lit * scale);
}
