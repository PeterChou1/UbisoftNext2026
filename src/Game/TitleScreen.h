#pragma once

#include "Camera.h"
#include "GameState.h"
#include "Lighting.h"
#include "OptionsMenuUI.h"
#include "Scene.h"
#include "TitleScreenCameraController.h"
#include "TitleScreenUI.h"

class TitleScreen : public Scene
{
  public:
    TitleScreen() = default;

    ~TitleScreen() override = default;

    void Start() override;

    void Setup() override;

    void Update(float deltaTime) override;

    void Render() override;

  private:
    std::shared_ptr<TitleScreenCameraController> m_CameraController;
    std::shared_ptr<GameOptions> m_GameOptions;
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<TitleScreenUI> m_TitleScreenUI;
    std::shared_ptr<OptionsMenuUI> m_OptionsMenuUI;
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
};
