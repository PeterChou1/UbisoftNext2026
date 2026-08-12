#pragma once
#include "Camera.h"
#include "GameState.h"
#include "UIState.h"

#include <memory>

class TitleScreenUI
{
  public:
    TitleScreenUI();

    void Render();

  private:
    std::shared_ptr<Camera> m_cam;
    std::shared_ptr<UIState> m_state;
    std::shared_ptr<GameState> m_gamestate;
};
