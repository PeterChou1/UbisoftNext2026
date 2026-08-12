#pragma once

#include "Camera.h"
#include "GameState.h"
#include "UIState.h"

#include <memory>

class MainLevelUI
{
  public:
    MainLevelUI();

    void Update();

    void Render();

  private:
    void RenderBaseContext();

    void RenderObstacleContext();

    std::shared_ptr<UIState> m_UIState;
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<Camera> m_Cam;
};
