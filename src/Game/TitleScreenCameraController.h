#pragma once
#include "Camera.h"
#include "GameOptions.h"
#include "GameState.h"

#include <memory>

class TitleScreenCameraController
{
  public:
    TitleScreenCameraController();

    void Update(float deltaTime);

  private:
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<Camera> m_Cam;
};
