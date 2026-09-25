//---------------------------------------------------------------------------------
// EffectShadersSIMD.h
//---------------------------------------------------------------------------------
//
// Animated fragment shaders that work on shapes and models alike. The surface
// colour is the vertex colour (shapes) or, when a triangle has none, the
// material's diffuse colour (.obj models); it is lit like ShapeShaderSIMD and
// then:
//
//   PulseShaderSIMD    the brightness pulses over time
//   RimShaderSIMD      edges facing away from the camera glow (rim light)
//   StripesShaderSIMD  horizontal bands scroll upwards (hologram / scanner)
//
// DeltaTime is the time the shader has run (seconds), advanced by the
// ShaderHandler.
//
#pragma once
#include "FragmentShader.h"

namespace EffectShading
{
    /**
     * \brief Vertex colour, or the material colour where it is black
     */
    SIMDVec3 BaseColor(const SIMDPixel& pixel, const Material& material);

    /**
     * \brief Ambient + Lambert diffuse from the directional light (0..1)
     */
    SIMDFloat Lighting(SIMDPixel& pixel, DirectionalLight& light);
} // namespace EffectShading

class PulseShaderSIMD : public FragmentShader
{
  public:
    static constexpr float SPEED = 4.0f;
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;
};

class RimShaderSIMD : public FragmentShader
{
  public:
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;
};

class StripesShaderSIMD : public FragmentShader
{
  public:
    // Bands per world unit, and how fast they move up (units per second)
    static constexpr float DENSITY = 2.0f;
    static constexpr float SPEED = 0.75f;
    void Shade(SIMDPixel& pixel,
               DepthBuffer& depthBuffer,
               Material& texture,
               Camera& camera,
               DirectionalLight& Light) override;
};
