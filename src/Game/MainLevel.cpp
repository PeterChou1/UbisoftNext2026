#include "MainLevel.h"

#include "BulletColliders.h"
#include "CreateMainLevel.h"
#include "GameManager.h"
#include "MainLevelUI.h"
#include "app.h"

extern ECSManager ECS;
extern GameManager GameSceneManager;

// Quick save / quick load keys
constexpr App::Key QUICK_SAVE_KEY = App::KEY_4;
constexpr App::Key QUICK_LOAD_KEY = App::KEY_5;

void MainLevel::Setup()
{
    // Set Original camera position
    float StartY = 10.0f;
    float StartZ = -5.0f;
    Vec3 StartCamera = {0, StartY, StartZ};
    Vec3 StartCameraTarget = {0.0f, 0.0, 0.0f};

    // Setup Game System
    m_GameOptions = ECS.GetResource<GameOptions>();
    m_ColliderCallback = ECS.GetResource<ColliderCallbackSystem>();
    m_GameState = ECS.GetResource<GameState>();
    m_Light = ECS.GetResource<Lighting>();
    m_Cam = ECS.GetResource<Camera>();
    m_ColliderCallback = ECS.GetResource<ColliderCallbackSystem>();
    m_CameraController = std::make_shared<GameCameraController>(StartY, StartZ);
    m_LevelUI = std::make_shared<MainLevelUI>();
    m_UnitControl = std::make_shared<UnitControllerSystem>();
    m_RoundController = std::make_shared<GameRoundControllerSystem>();
    m_ProjectileController = std::make_shared<ProjectileControllerSystem>();
    m_BuildObstacleSystem = std::make_shared<BuildObstaclesSystem>();
    // Set Lighting parameters
    m_Light->SetLightPerspective(120.0f, m_GameOptions->ScreenRatio, 0.1f, 1000.0f);
    m_Light->SetPositionAndTarget(Vec3(0.0f, 25.0f, -5.0f), Vec3(0.0f, 0.0f, 0.0f));
    m_Cam->SetPositionAndOrientation(StartCamera, StartCameraTarget, {0, 1, 0});

    // Register Collider callback
    m_ColliderCallback->RegisterCallback(std::make_shared<BulletUnitCollider>());
    m_ColliderCallback->RegisterCallback(std::make_shared<ExplosionUnitCollider>());

    CreateMainLevel();
}

void MainLevel::Update(float deltaTime)
{
    HandleSaveKeys();
    m_LevelUI->Update();
    m_CameraController->Update(deltaTime);
    m_UnitControl->Update();
    m_RoundController->Update(deltaTime);
    m_ProjectileController->Update(deltaTime);
    m_BuildObstacleSystem->Update(deltaTime);
    // m_DebugCamera->Update(deltaTime);
}

void MainLevel::Render()
{
    m_LevelUI->Render();
}

void MainLevel::OnWorldRestored()
{
    RestoreMainLevelRuntimeState();
}

void MainLevel::HandleSaveKeys()
{
    bool saveDown = App::IsKeyPressed(QUICK_SAVE_KEY);
    bool loadDown = App::IsKeyPressed(QUICK_LOAD_KEY);
    // The actual save / load happens at the start of the next frame
    if (saveDown && !m_QuickSaveHeld)
        GameSceneManager.RequestSave();
    if (loadDown && !m_QuickLoadHeld)
        GameSceneManager.RequestLoad();
    m_QuickSaveHeld = saveDown;
    m_QuickLoadHeld = loadDown;
}
