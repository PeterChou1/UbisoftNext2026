#include "EffectShadersSIMD.h"

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

    SIMDFloat Lighting(SIMDPixel& pixel, DirectionalLight& light)
    {
        const SIMDFloat ambient = 0.45f;
        SIMDVec3 lightPos = light.Position;
        SIMDVec3 normal = pixel.Normal.Normalize();
        SIMDVec3 lightDir = (lightPos - pixel.WorldSpacePosition).Normalize();
        SIMDFloat diffuse = SIMD::Max(normal.Dot(lightDir), SIMD::ZERO);
        return SIMD::Clamp(SIMD::ZERO, SIMD::ONE, ambient + diffuse * 0.65f);
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
    SIMDVec3 lit = EffectShading::BaseColor(pixel, texture) * EffectShading::Lighting(pixel, Light);
    // Between 70% of the lit colour and a bright, whitened version of it
    SIMDFloat scale = 0.7f + 0.6f * wave;
    SIMDFloat white = 0.25f * wave;
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
    SIMDVec3 lit = base * EffectShading::Lighting(pixel, Light);
    SIMDVec3 camPos = camera.Position;
    SIMDVec3 normal = pixel.Normal.Normalize();
    SIMDVec3 viewDir = (camPos - pixel.WorldSpacePosition).Normalize();
    // 0 facing the camera, 1 at grazing angles; cubed to keep it on the edges
    SIMDFloat facing = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, SIMD::Abs(normal.Dot(viewDir)));
    SIMDFloat rim = SIMD::ONE - facing;
    rim = rim * rim * rim;
    // The rim is a light tint of the base colour
    SIMDVec3 glow = base * SIMDFloat(0.5f) + SIMDVec3(0.5f, 0.5f, 0.5f);
    SIMDVec3 color = lit + glow * rim;
    pixel.Color = Saturate(color);
}

void StripesShaderSIMD::Shade(SIMDPixel& pixel,
                              DepthBuffer& depthBuffer,
                              Material& texture,
                              Camera& camera,
                              DirectionalLight& Light)
{
    SIMDVec3 lit = EffectShading::BaseColor(pixel, texture) * EffectShading::Lighting(pixel, Light);
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
