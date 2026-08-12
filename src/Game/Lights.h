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
    LightType lightType = SpotLight;

    void SetColor(float r, float g, float b);

    void SetPositionAndTarget(Vec3& Pos, Vec3& Target);

    void SetLightPerspective(float fov, float aspect, float near, float far);

    void SetLightOrthogonal(Vec2& Max, Vec2& Min, float near, float far);

    Vec3 WorldToLightSpace(Vec3& Pos);

    void Update(Vec3& Delta, Quat& Rot);
};
