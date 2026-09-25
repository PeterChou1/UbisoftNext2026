#include "EffectVertexShaders.h"

#include "stdafx.h"

#include <cmath>

namespace VertexShading
{
    void Project(Vertex& v, Vec3 world, Camera& cam, DirectionalLight& light, bool shadows)
    {
        v.PositionCamera = cam.WorldToCamera(world);
        v.Projection = cam.Proj * Vec4(v.PositionCamera);
        if (shadows)
        {
            Vec3 lightSpace = light.WorldToLightSpace(world);
            v.ShadowProjection = light.Proj * Vec4(lightSpace);
        }
    }
} // namespace VertexShading

Vec3 WaveVertexShader::Displace(const Vertex& v, float t)
{
    const Vec3& p = v.Position;
    float offset = AMPLITUDE * std::sin(t * SPEED + (p.X + p.Z) * FREQUENCY);
    return {p.X, p.Y + offset, p.Z};
}

void WaveVertexShader::Shade(Vertex& v, Camera& cam, DirectionalLight& light)
{
    VertexShading::Project(v, Displace(v, DeltaTime), cam, light, ShadowMapping);
}

Vec3 SwayVertexShader::Displace(const Vertex& v, float t)
{
    const Vec3& p = v.Position;
    // Only what is above the model's origin moves; a small phase from the
    // world position keeps neighbours out of step
    float height = v.LocalPosition.Y > 0.0f ? v.LocalPosition.Y : 0.0f;
    float phase = t * SPEED + (p.X - v.LocalPosition.X) * 0.3f + (p.Z - v.LocalPosition.Z) * 0.3f;
    float amount = STRENGTH * height;
    return {p.X + amount * std::sin(phase), p.Y, p.Z + amount * 0.5f * std::cos(phase * 0.7f)};
}

void SwayVertexShader::Shade(Vertex& v, Camera& cam, DirectionalLight& light)
{
    VertexShading::Project(v, Displace(v, DeltaTime), cam, light, ShadowMapping);
}
