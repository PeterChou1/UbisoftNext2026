#include "EffectShadersSIMD.h"

#include "ShadowSampling.h"
#include "stdafx.h"

#include <cmath>

namespace
{
    // SIMD::Select takes non-const references in the AVX2 build
    SIMDFloat Pick(SIMDFloat mask, SIMDFloat whenFalse, SIMDFloat whenTrue)
    {
        return SIMD::Select(mask, whenFalse, whenTrue);
    }

    SIMDVec3 Saturate(const SIMDVec3& color)
    {
        return {SIMD::Min(color.X, SIMD::ONE), SIMD::Min(color.Y, SIMD::ONE), SIMD::Min(color.Z, SIMD::ONE)};
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
        return {Pick(black, vertex.X, r / 255.0f),
                Pick(black, vertex.Y, g / 255.0f),
                Pick(black, vertex.Z, b / 255.0f)};
    }

    SIMDFloat Lighting(SIMDPixel& pixel, DirectionalLight& light, const DepthBuffer& depthBuffer, bool shadows)
    {
        const SIMDFloat ambient = light.Ambient;
        SIMDVec3 lightPos = light.Position;
        SIMDVec3 normal = pixel.Normal.Normalize();
        SIMDVec3 lightDir = (lightPos - pixel.WorldSpacePosition).Normalize();
        SIMDFloat diffuse = SIMD::Max(normal.Dot(lightDir), SIMD::ZERO) * (0.65f * light.Intensity);
        if (shadows)
            diffuse = diffuse * ShadowSampling::Visibility(pixel, depthBuffer, light);
        return SIMD::Clamp(SIMD::ZERO, SIMD::ONE, ambient + diffuse);
    }

    SIMDVec3 LightColor(const DirectionalLight& light)
    {
        return SIMDVec3(light.Color.X, light.Color.Y, light.Color.Z);
    }
} // namespace EffectShading

void PulseShaderSIMD::Shade(SIMDPixel& pixel,
                            DepthBuffer& depthBuffer,
                            Material& texture,
                            Camera& camera,
                            DirectionalLight& Light)
{
    // One value per draw: the whole object pulses together
    const float wave = 0.5f + 0.5f * std::sin(DeltaTime * SPEED);
    SIMDVec3 lit = EffectShading::BaseColor(pixel, texture) * EffectShading::Lighting(pixel, Light, depthBuffer, ShadowMapping) *
                   EffectShading::LightColor(Light);
    // From 55% of the lit colour up to all of it plus a white glow: dimming
    // (not only brightening) keeps the pulse visible on bright surfaces,
    // which would otherwise stay clipped at white
    SIMDFloat scale = 0.55f + 0.45f * wave;
    SIMDFloat white = 0.15f * wave;
    SIMDVec3 color = lit * scale + SIMDVec3(white, white, white);
    pixel.Color = Saturate(color);
}

void RimShaderSIMD::Shade(SIMDPixel& pixel,
                          DepthBuffer& depthBuffer,
                          Material& texture,
                          Camera& camera,
                          DirectionalLight& Light)
{
    SIMDVec3 base = EffectShading::BaseColor(pixel, texture);
    SIMDVec3 lit = base * EffectShading::Lighting(pixel, Light, depthBuffer, ShadowMapping) *
                   EffectShading::LightColor(Light);
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
    SIMDVec3 color = lit * SIMDFloat(0.85f) + glow * rim;
    pixel.Color = Saturate(color);
}

void StripesShaderSIMD::Shade(SIMDPixel& pixel,
                              DepthBuffer& depthBuffer,
                              Material& texture,
                              Camera& camera,
                              DirectionalLight& Light)
{
    SIMDVec3 lit = EffectShading::BaseColor(pixel, texture) * EffectShading::Lighting(pixel, Light, depthBuffer, ShadowMapping) *
                   EffectShading::LightColor(Light);
    // Position in the band pattern, 0..1 inside each band
    SIMDFloat band = pixel.WorldSpacePosition.Y * DENSITY - SIMDFloat(DeltaTime * SPEED * DENSITY);
    SIMDFloat whole = band;
    SIMDFloat fraction = band - whole.floor();
    // Bright half and dim half of every band
    SIMDFloat bright = fraction < SIMDFloat(0.5f);
    SIMDFloat dim = 0.45f;
    SIMDFloat full = 1.15f;
    SIMDFloat scale = Pick(bright, dim, full);
    SIMDVec3 color = lit * scale;
    pixel.Color = Saturate(color);
}
