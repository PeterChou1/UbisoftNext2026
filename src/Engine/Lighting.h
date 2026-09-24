#pragma once

#include "Lights.h"
#include "Resource.h"

class Lighting : public Resource
{
  public:
    void ResetResource() override { DirectionalLight = {}; }

    void SetPositionAndTarget(Vec3 Position, Vec3 Target)
    {
        DirectionalLight.SetPositionAndTarget(Position, Target);
    }

    void SetLightPerspective(float FOV, float Aspect, float Near, float Far)
    {
        DirectionalLight.lightType = SpotLight;
        DirectionalLight.SetLightPerspective(FOV, Aspect, Near, Far);
    }

    void SetLightOrthogonal(Vec2 Max, Vec2 Min, float Near, float Far)
    {
        // NOTE: orthogonal parallel lights don't work currently
        DirectionalLight.lightType = ParallelLight;
        DirectionalLight.SetLightOrthogonal(Max, Min, Near, Far);
    }

    DirectionalLight& GetDirectionalLight() { return DirectionalLight; }

    bool IsPerspective() { return DirectionalLight.lightType == SpotLight; }

  private:
    DirectionalLight DirectionalLight;
};
