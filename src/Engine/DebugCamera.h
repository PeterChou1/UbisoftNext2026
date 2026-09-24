//---------------------------------------------------------------------------------
// DebugCamera.h
//---------------------------------------------------------------------------------
//
// Basic Camera System used for debugging not for gameplay use
//
#pragma once
#include "Camera.h"

#include <memory>

class DebugCamera
{
  public:
    DebugCamera();

    void Update(float deltaTime);
    void Render();

  private:
    std::shared_ptr<Camera> m_Cam;
};
