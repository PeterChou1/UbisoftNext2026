//---------------------------------------------------------------------------------
// Lights.h
//---------------------------------------------------------------------------------
//
// Lights used in the rendering System
//
#pragma once

#include "Transform.h"
#include "Vec3.h"

enum LightType
{
    // Normal spot light
    SpotLight,
    // Simulate light source infinitely far away
    ParallelLight,
};

struct DirectionalLight
{

    Mat4 Proj{};
    Transform LightTransform{};
    Vec3 Position;
    Vec3 Color = {1, 1, 1};
    // Brightness of the direct light, and the light every surface gets even
    // in shadow (set from the scene's light object, SceneLight.h)
    float Intensity = 1.0f;
    float Ambient = 0.45f;
    // Unit direction the light shines in (from Position towards its target)
    Vec3 Direction = {0, -1, 0};
    // Spot lights: cosines of the angles where the light starts to fade and
    // where it is gone (the edge of the cone). -1 / -1: no cone
    float SpotCosInner = -1.0f;
    float SpotCosOuter = -1.0f;
    // Shadow map lookups (ShadowSampling): the world size of one shadow map
    // texel (parallel light) or its size per unit of distance (spot light),
    // and how much the stored depth changes per world unit (parallel light)
    float TexelSize = 0.1f;
    float DepthPerUnit = 0.01f;
    LightType lightType = SpotLight;

    void SetColor(float r, float g, float b);

    void SetPositionAndTarget(Vec3& Pos, Vec3& Target);

    void SetLightPerspective(float fov, float aspect, float near, float far);

    void SetLightOrthogonal(Vec2& Max, Vec2& Min, float near, float far);

    /**
     * \brief Parallel light: an orthographic box in light space (the light
     *        looks down its -Z axis; near / far are distances along it)
     */
    void SetOrthographic(float left, float right, float bottom, float top, float near, float far);

    Vec3 WorldToLightSpace(Vec3& Pos);

    void Update(Vec3& Delta, Quat& Rot);
};
