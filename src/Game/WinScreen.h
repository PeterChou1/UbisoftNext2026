#pragma once

#include "GameState.h"
#include "Scene.h"
#include "TitleScreenUI.h"

class WinScreen : public Scene
{
  public:
    WinScreen() = default;

    ~WinScreen() override = default;

    void Start() override;

    void Render() override;

  private:
    std::shared_ptr<UIState> m_uistate;
    std::shared_ptr<GameState> m_gamestate;
};
