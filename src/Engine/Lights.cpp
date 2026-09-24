#include "Lights.h"

#include "stdafx.h"

void DirectionalLight::SetColor(float r, float g, float b)
{
    Color = Vec3(r, g, b);
}

void DirectionalLight::SetPositionAndTarget(Vec3& Pos, Vec3& Target)
{
    Position = Pos;
    LightTransform = Transform(Position, Target, {0, 1, 0});
}

void DirectionalLight::SetLightPerspective(float fov, float aspect, float near, float far)
{
    lightType = SpotLight;
    Proj.PerspectiveOpenGL(fov, aspect, near, far);
}

void DirectionalLight::SetLightOrthogonal(Vec2& Max, Vec2& Min, float near, float far)
{
    lightType = ParallelLight;
    Proj.OrthogonalOpenGL(Min.X, Min.Y, Max.X, Max.Y, near, far);
}

Vec3 DirectionalLight::WorldToLightSpace(Vec3& Pos)
{
    return LightTransform.Inverse * Pos;
}

void DirectionalLight::Update(Vec3& Delta, Quat& Rot)
{
    Position = Position + Delta;
    LightTransform.Update(Delta, Rot);
}
