//---------------------------------------------------------------------------------
// DebugPhysicsRenderer.h
//---------------------------------------------------------------------------------
//
// Basic debug System used to render physics object not for gameplay use
//
#pragma once
#include "Camera.h"
#include "Lighting.h"

#include <memory>

class DebugPhysicsRenderer
{
  public:
    DebugPhysicsRenderer();

    void Update(float deltaTime);

    void Render();

  private:
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_light;
    float accumulate{};
};
