#include "BlingPhong.h"

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
    SIMDVec3 lightPos = Light.Position;
    SIMDVec3 lightColor = Light.Color * Light.Intensity;
    SIMDFloat shininess = texture.highlight;
    // Materials without an ambient term (the default material of shapes)
    // get the scene light's, so shadows are dark but not black
    Vec3 ambient = texture.ambient;
    if (ambient.X + ambient.Y + ambient.Z <= 0.0f)
        ambient = Vec3(Light.Ambient, Light.Ambient, Light.Ambient);
    SIMDVec3 ambientColor = ambient;
    SIMDVec3 diffuseColor = texture.diffuse;
    SIMDVec3 specularColor = texture.specular;
    SIMDVec3 fragPos = pixel.WorldSpacePosition;
    // Phong Shading Model
    SIMDFloat r, g, b;
    texture.SampleSIMD(r, g, b);
    SIMDVec3 mappedDiffuseColor = SIMDVec3(r / 255.0, g / 255.0, b / 255.0);
    SIMDVec3 normal = pixel.Normal.Normalize();
    SIMDVec3 lightDir = (lightPos - fragPos).Normalize();
    SIMDVec3 viewDir = (camPos - fragPos).Normalize();
    SIMDFloat diff = normal.Dot(lightDir);
    diff = SIMD::Max(diff, SIMD::ZERO);
    // Bling Phong Model
    SIMDVec3 halfwayDir = (lightDir + viewDir).Normalize();
    SIMDFloat spec = SIMD::Max(normal.Dot(halfwayDir), SIMD::ZERO);
    // (pow returns the result)
    spec = spec.pow(shininess);
    SIMDVec3 specular = lightColor * specularColor * spec;
    SIMDVec3 diffuse = lightColor * diff;
    // Shadow Map
    // 1 = lit, 0 = in the shadow of something closer to the light
    SIMDFloat shadow = SIMD::ONE;
    if (ShadowMapping)
        shadow = ShadowSampling::Visibility(pixel, depthBuffer, Light);
    pixel.Color =
            (ambientColor + (diffuse + specular) * shadow) * mappedDiffuseColor * diffuseColor;
    // Clamp the pixel colors to 0 -> 1
    pixel.Color.X = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.X);
    pixel.Color.Y = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.Y);
    pixel.Color.Z = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.Z);
}
