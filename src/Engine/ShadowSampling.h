//---------------------------------------------------------------------------------
// ShadowSampling.h
//---------------------------------------------------------------------------------
//
// Shadow map lookup shared by the lit fragment shaders (Blinn-Phong, Shape and
// the effect shaders).
//
// With GameOptions::ShadowMapping the rasterizer also draws every triangle
// from the light (ClipperSystem's light pass, RasterizerSystem's shadow
// tiles) into the DepthBuffer's shadow buffer, keeping the largest 1 / w
// (the closest surface to the light) at each texel. A pixel is in shadow
// when the shadow map holds something clearly closer to the light than the
// pixel itself.
//
#pragma once

#include "DepthBuffer.h"
#include "Lights.h"
#include "SIMDPixel.h"

namespace ShadowSampling
{
    // A pixel is shadowed when the closest 1 / w seen by the light is this
    // much larger (relative) than its own: keeps surfaces from shadowing
    // themselves (shadow acne)
    constexpr float BIAS = 0.015f;

    /**
     * \brief 1 where the 8 pixels are lit by the light, 0 where something
     *        between them and the light casts a shadow. Pixels outside the
     *        light's view are lit
     */
    SIMDFloat Visibility(const SIMDPixel& pixel, const DepthBuffer& depthBuffer, DirectionalLight& light);
} // namespace ShadowSampling
