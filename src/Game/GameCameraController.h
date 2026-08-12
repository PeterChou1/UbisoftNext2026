#pragma once
#include "Camera.h"
#include "GameOptions.h"
#include "GameState.h"

#include <memory>

class GameCameraController
{
  public:
    GameCameraController(){};

    GameCameraController(float StartX, float StartY);

    void Update(float deltaTime);

  private:
    float StartX, StartY;
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<Camera> m_Cam;
};
