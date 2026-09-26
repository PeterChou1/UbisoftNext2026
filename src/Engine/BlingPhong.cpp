#include "BlingPhong.h"

#include "EffectShadersSIMD.h"
#include "ShadowSampling.h"
#include "stdafx.h"

void BlinnPhongSIMD::Shade(SIMDPixel& pixel,
                           DepthBuffer& depthBuffer,
                           Material& texture,
                           Camera& camera,
                           DirectionalLight& light)
{
    SIMDVec3 camPos = camera.Position;
    SIMDVec3 lightColor = light.Color * light.Intensity;
    SIMDFloat shininess = texture.highlight;
    // The scene light's ambient, scaled by the material's (materials without
    // one, like the default material of shapes, take it as is), so shadows
    // are dark but not black
    Vec3 ambient = texture.ambient;
    if (ambient.X + ambient.Y + ambient.Z <= 0.0f)
        ambient = Vec3(1.0f, 1.0f, 1.0f);
    SIMDVec3 ambientColor = ambient * light.Ambient;
    SIMDVec3 specularColor = texture.specular;
    SIMDFloat r, g, b;
    texture.SampleSIMD(r, g, b);
    SIMDVec3 mappedDiffuseColor = SIMDVec3(r / 255.0, g / 255.0, b / 255.0);
    SIMDVec3 normal = pixel.Normal.Normalize();
    // Parallel light: its direction; spot light: towards it, faded at the
    // edge of its cone
    SIMDFloat cone;
    SIMDVec3 lightDir = EffectShading::ToLight(pixel, light, cone);
    SIMDVec3 viewDir = (camPos - pixel.WorldSpacePosition).Normalize();
    SIMDFloat diff = SIMD::Max(normal.Dot(lightDir), SIMD::ZERO);
    SIMDVec3 halfwayDir = (lightDir + viewDir).Normalize();
    SIMDFloat spec = SIMD::Max(normal.Dot(halfwayDir), SIMD::ZERO).pow(shininess);
    SIMDVec3 specular = lightColor * specularColor * (spec * cone);
    // (scaled like the shape shaders: ambient + 0.65 diffuse)
    SIMDVec3 diffuse = lightColor * (diff * cone * 0.65f);
    // 1 = lit, 0 = in the shadow of something closer to the light
    SIMDFloat shadow = SIMD::ONE;
    if (ShadowMapping)
        shadow = ShadowSampling::Visibility(pixel, depthBuffer, light);
    // The surface colour is the material's diffuse colour (SampleSIMD); it
    // used to be multiplied by it a second time, which made dark materials
    // almost black. Highlights are the light's colour, not the surface's
    SIMDVec3 color = (ambientColor + diffuse * shadow) * mappedDiffuseColor + specular * shadow;
    pixel.Color.X = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, color.X);
    pixel.Color.Y = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, color.Y);
    pixel.Color.Z = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, color.Z);
}
