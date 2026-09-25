#include "BlingPhong.h"

#include "EffectShadersSIMD.h"
#include "ShadowSampling.h"

#include "stdafx.h"

void BlinnPhongSIMD::Shade(SIMDPixel& pixel,
                           DepthBuffer& depthBuffer,
                           Material& texture,
                           Camera& cam,
                           DirectionalLight& Light)
{
    // setup
    SIMDVec3 camPos = cam.Position;
    SIMDVec3 lightColor = Light.Color * Light.Intensity;
    SIMDFloat shininess = texture.highlight;
    // The scene light's ambient, scaled by the material's (materials without
    // one, like the default material of shapes, take it as is), so shadows
    // are dark but not black
    Vec3 ambient = texture.ambient;
    if (ambient.X + ambient.Y + ambient.Z <= 0.0f)
        ambient = Vec3(1.0f, 1.0f, 1.0f);
    SIMDVec3 ambientColor = ambient * Light.Ambient;
    SIMDVec3 specularColor = texture.specular;
    SIMDVec3 fragPos = pixel.WorldSpacePosition;
    // Phong Shading Model
    SIMDFloat r, g, b;
    texture.SampleSIMD(r, g, b);
    SIMDVec3 mappedDiffuseColor = SIMDVec3(r / 255.0, g / 255.0, b / 255.0);
    SIMDVec3 normal = pixel.Normal.Normalize();
    // Parallel light: its direction; spot light: towards it, faded at the
    // edge of its cone
    SIMDFloat cone;
    SIMDVec3 lightDir = EffectShading::ToLight(pixel, Light, cone);
    SIMDVec3 viewDir = (camPos - fragPos).Normalize();
    SIMDFloat diff = normal.Dot(lightDir);
    diff = SIMD::Max(diff, SIMD::ZERO);
    // Bling Phong Model
    SIMDVec3 halfwayDir = (lightDir + viewDir).Normalize();
    SIMDFloat spec = SIMD::Max(normal.Dot(halfwayDir), SIMD::ZERO);
    // (pow returns the result)
    spec = spec.pow(shininess);
    SIMDVec3 specular = lightColor * specularColor * (spec * cone);
    // (scaled like the shape shaders: ambient + 0.65 diffuse)
    SIMDVec3 diffuse = lightColor * (diff * cone * 0.65f);
    // Shadow Map
    // 1 = lit, 0 = in the shadow of something closer to the light
    SIMDFloat shadow = SIMD::ONE;
    if (ShadowMapping)
        shadow = ShadowSampling::Visibility(pixel, depthBuffer, Light);
    // The surface colour is the material's diffuse colour (SampleSIMD); it
    // used to be multiplied by it a second time, which made dark materials
    // almost black. Highlights are the light's colour, not the surface's
    pixel.Color = (ambientColor + diffuse * shadow) * mappedDiffuseColor + specular * shadow;
    // Clamp the pixel colors to 0 -> 1
    pixel.Color.X = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.X);
    pixel.Color.Y = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.Y);
    pixel.Color.Z = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.Z);
}
