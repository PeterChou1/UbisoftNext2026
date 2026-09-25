#include "ShapeShaderSIMD.h"

#include "EffectShadersSIMD.h"

#include "stdafx.h"

void ShapeShaderSIMD::Shade(SIMDPixel& pixel,
                            DepthBuffer& depthBuffer,
                            Material& texture,
                            Camera& camera,
                            DirectionalLight& Light)
{
    // Lambert diffuse + ambient so the top and the sides of an extruded shape
    // read differently, the colour comes from the vertices. Shadows when the
    // shadow map is on
    SIMDFloat intensity = EffectShading::Lighting(pixel, Light, depthBuffer, ShadowMapping);
    pixel.Color = pixel.VertexColor * intensity * EffectShading::LightColor(Light);
}
