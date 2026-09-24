#include "ShapeShaderSIMD.h"

#include "stdafx.h"

void ShapeShaderSIMD::Shade(SIMDPixel& pixel,
                            DepthBuffer& depthBuffer,
                            Material& texture,
                            Camera& camera,
                            DirectionalLight& Light)
{
    // Lambert diffuse + ambient so the top and the sides of an extruded shape
    // read differently, the colour comes from the vertices
    const SIMDFloat ambient = 0.45f;
    SIMDVec3 lightPos = Light.Position;
    SIMDVec3 normal = pixel.Normal.Normalize();
    SIMDVec3 lightDir = (lightPos - pixel.WorldSpacePosition).Normalize();
    SIMDFloat diffuse = SIMD::Max(normal.Dot(lightDir), SIMD::ZERO);
    SIMDFloat intensity = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, ambient + diffuse * 0.65f);
    pixel.Color = pixel.VertexColor * intensity;
}
