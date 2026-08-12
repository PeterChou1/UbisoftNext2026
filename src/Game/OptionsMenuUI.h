#pragma once

#include "Camera.h"
#include "DepthBuffer.h"
#include "GameOptions.h"
#include "GameState.h"
#include "Tiles.h"
#include "UIState.h"

#include <memory>

class OptionsMenuUI
{
  public:
    OptionsMenuUI();

    void Render();

  private:
    std::shared_ptr<UIState> m_state;
    std::shared_ptr<GameOptions> m_gameoptions;
    std::shared_ptr<GameState> m_gamestate;
    std::shared_ptr<Camera> m_cam;
    std::shared_ptr<DepthBuffer> m_depth;
    std::shared_ptr<Tiles> m_Tiles;
};