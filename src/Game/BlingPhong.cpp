#include "BlingPhong.h"

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
    SIMDVec3 lightColor = Light.Color;
    SIMDFloat shininess = texture.highlight;
    SIMDVec3 ambientColor = texture.ambient;
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
    spec.pow(shininess);
    SIMDVec3 specular = lightColor * specularColor * spec;
    SIMDVec3 diffuse = lightColor * diff;
    // Shadow Map
    SIMDFloat realCurrentDepth;
    SIMDVec2 realShadowCoords;
    SIMDFloat shadow = SIMD::ONE;
    if (ShadowMapping)
    {
        for (int x = 0; x < 8; x++)
        {
            Vec3 position = Vec3(pixel.WorldSpacePosition.X.V[x],
                                 pixel.WorldSpacePosition.Y.V[x],
                                 pixel.WorldSpacePosition.Z.V[x]);
            Vec3 lightSpace = Light.WorldToLightSpace(position);
            Vec4 Proj = Light.Proj * Vec4(lightSpace);
            float w = 1 / Proj.W;
            // Check if we are behind the light or w == 0
            if (Proj.W <= 0.0f)
            {
                // Mark pixel as out-of-frustum
                realCurrentDepth.V[x] = 0.0f;
                realShadowCoords.X.V[x] = -1.0f;
                realShadowCoords.Y.V[x] = -1.0f;
                continue;
            }

            // Perspective divide => Normalized Device Coordinates (NDC)
            float ndcX = Proj.X / Proj.W;
            float ndcY = Proj.Y / Proj.W;
            float ndcZ = Proj.Z / Proj.W;

            // Check if the pixel is inside -1..1 for X and Y, and 0..1 for Z
            if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f || ndcZ < -1.0f ||
                ndcZ > 1.0f)
            {
                // Outside the light's frustum
                realCurrentDepth.V[x] = 0.0f;
                realShadowCoords.X.V[x] = -1.0f;
                realShadowCoords.Y.V[x] = -1.0f;
                continue;
            }
            realCurrentDepth.V[x] = w;
            Proj.X *= w;
            Proj.Y *= w;
            Proj.Z *= w;
            depthBuffer.ToShadowSpace(Proj);
            realShadowCoords.X.V[x] = Proj.X;
            realShadowCoords.Y.V[x] = Proj.Y;
        }
        realCurrentDepth = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, realCurrentDepth);
        SIMDFloat closestDepth = depthBuffer.GetDepthSIMD(realShadowCoords, true);
        closestDepth = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, closestDepth);
        SIMDFloat closest = closestDepth > realCurrentDepth;
        shadow = SIMD::Select(
                SIMD::Abs(closestDepth - realCurrentDepth) < 0.001f, closest, SIMD::ZERO);
        shadow = SIMDFloat(1.0f) - shadow;
        shadow = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, shadow);
    }
    pixel.Color =
            (ambientColor + (diffuse + specular) * shadow) * mappedDiffuseColor * diffuseColor;
    // Clamp the pixel colors to 0 -> 1
    pixel.Color.X = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.X);
    pixel.Color.Y = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.Y);
    pixel.Color.Z = SIMD::Clamp(SIMD::ZERO, SIMD::ONE, pixel.Color.Z);
}
