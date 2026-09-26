//---------------------------------------------------------------------------------
// ShadowSampling.h
//---------------------------------------------------------------------------------
//
// Shadow map lookup shared by the lit fragment shaders (Blinn-Phong, Shape and
// the effect shaders).
//
// With shadows on (GameOptions::ShadowsOn) the rasterizer also draws every
// triangle from the light (ClipperSystem's light pass, RasterizerSystem's
// shadow tiles) into the DepthBuffer's shadow buffer. It keeps, at each
// texel, the surface closest to the light:
//
//   parallel light  (1 - NDC z) / 2 of an orthographic box fitted around the
//                   scene (SceneLighting): 1 near the light .. 0 far
//   spot light      1 / w of its perspective cone
//
// A pixel is in shadow when the map holds something closer to the light than
// the pixel itself. Two things keep this clean:
//
//   - normal offset: the pixel is looked up from a point moved a texel and a
//     half along its surface normal, so a surface never shadows itself
//     (shadow acne), even when the light grazes it, with only a tiny depth
//     bias that grows only where the light grazes a surface (so shadows stay
//     attached to what casts them)
//   - filtering: the 2 x 2 texels around the lookup are compared and their
//     results blended by distance (percentage closer filtering), so shadow
//     edges are smooth instead of stair-stepped where one texel covers
//     several screen pixels
//
#pragma once

#include "DepthBuffer.h"
#include "Lights.h"
#include "SIMDPixel.h"

namespace ShadowSampling
{
    // Look-up point offset along the surface normal, in shadow map texels
    constexpr float NORMAL_OFFSET = 1.5f;
    // Depth bias: world units (parallel light), relative 1 / w (spot light)
    constexpr float PARALLEL_BIAS = 0.02f;
    constexpr float SPOT_BIAS = 0.002f;
    // Extra depth bias where the light grazes a surface: this many texels
    // times the slope (tangent of the light's angle to the normal), capped
    constexpr float SLOPE_BIAS = 1.0f;
    constexpr float MAX_SLOPE = 4.0f;

    /**
     * \brief How much a point is lit (1) or in shadow (0), blended at shadow
     *        edges. Points outside the shadow map are lit
     */
    float Visibility(const Vec3& position, const Vec3& normal, const DepthBuffer& depthBuffer, DirectionalLight& light);

    /**
     * \brief Visibility of the 8 pixels
     */
    SIMDFloat Visibility(const SIMDPixel& pixel, const DepthBuffer& depthBuffer, DirectionalLight& light);
} // namespace ShadowSampling
