#pragma once

#include "BuildObstaclesSystem.h"
#include "Bullet.h"
#include "ColliderCallbackSystem.h"
#include "DebugCamera.h"
#include "GameCameraController.h"
#include "GameRoundControllerSystem.h"
#include "Lighting.h"
#include "MainLevelUI.h"
#include "ProjectileControllerSystem.h"
#include "Scene.h"
#include "UnitControllerSystem.h"

class MainLevel : public Scene
{
  public:
    void Setup() override;

    void Update(float deltaTime) override;

    void Render() override;

    void OnWorldRestored() override;

  private:
    void HandleSaveKeys();

    // Keys are polled every frame, remember the previous state so holding a
    // key only triggers a single save / load
    bool m_QuickSaveHeld = false;
    bool m_QuickLoadHeld = false;

    std::shared_ptr<ColliderCallbackSystem> m_ColliderCallback;
    std::shared_ptr<GameOptions> m_GameOptions;
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<HandleTankProjectiles> m_HandleExplosion;
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<GameCameraController> m_CameraController;
    std::shared_ptr<UnitControllerSystem> m_UnitControl;
    std::shared_ptr<GameRoundControllerSystem> m_RoundController;
    std::shared_ptr<ProjectileControllerSystem> m_ProjectileController;
    std::shared_ptr<BuildObstaclesSystem> m_BuildObstacleSystem;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<MainLevelUI> m_LevelUI;
};
