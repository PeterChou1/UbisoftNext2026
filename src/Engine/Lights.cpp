#include "Lights.h"

#include "stdafx.h"

#include <cmath>

void DirectionalLight::SetColor(float r, float g, float b)
{
    Color = Vec3(r, g, b);
}

void DirectionalLight::SetPositionAndTarget(Vec3& Pos, Vec3& Target)
{
    Position = Pos;
    Vec3 toTarget = Target - Pos;
    if (toTarget.Dot(toTarget) > 1e-12f)
    {
        Direction = toTarget;
        Direction.Normalize();
    }
    // Looking (almost) straight down: "up" can not be the Y axis
    Vec3 up = std::fabs(Direction.Y) > 0.999f ? Vec3(0, 0, 1) : Vec3(0, 1, 0);
    LightTransform = Transform(Position, Target, up);
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

void DirectionalLight::SetOrthographic(float left, float right, float bottom, float top, float near, float far)
{
    lightType = ParallelLight;
    // (Mat4::OrthogonalOpenGL takes bottom, left, top, right)
    Proj.OrthogonalOpenGL(bottom, left, top, right, near, far);
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
