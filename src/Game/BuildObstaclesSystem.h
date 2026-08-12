#pragma once

#include "BlackBoard.h"
#include "Camera.h"
#include "GameState.h"
#include "UIState.h"

class BuildObstaclesSystem
{
  public:
    BuildObstaclesSystem();

    void Update(float deltaTime);

  private:
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<UIState> m_UIstate;
    std::shared_ptr<GameState> m_GameState;
};
