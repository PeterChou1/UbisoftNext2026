#include "TitleScreen.h"

#include "AssetServer.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "GameManager.h"
#include "GameUtils.h"
#include "stdafx.h"

extern ECSManager ECS;
extern GameManager GameSceneManager;

void TitleScreen::Start()
{
    m_Cam = ECS.GetResource<Camera>();
    m_Light = ECS.GetResource<Lighting>();
    m_GameOptions = ECS.GetResource<GameOptions>();
    m_GameState = ECS.GetResource<GameState>();
    m_TitleScreenUI = std::make_shared<TitleScreenUI>();
    m_OptionsMenuUI = std::make_shared<OptionsMenuUI>();
    m_CameraController = std::make_shared<TitleScreenCameraController>();
}

void TitleScreen::Setup()
{
    m_Light->SetLightPerspective(120.0f, m_GameOptions->ScreenRatio, 0.1f, 1000.0f);
    m_Light->SetPositionAndTarget(Vec3(0.0f, 5.0f, -5.0f), Vec3(0.0f, 0.0f, 0.0f));
    auto& server = AssetServer::GetInstance();
    server.LoadLevelAssets({TitleScreenBackground, GoalPost, WordLogo});
    m_Cam->SetProjectionPerspective();
    m_Cam->SetPositionAndOrientation({0, 5, -1}, {0.0f, 0.0, 3.0f}, {0, 1, 0});

    CreateMeshEntity({0, 0, 20}, WordLogo, Quat({0, 1, 0}, 3.141f), {0.1f, 0.1f, 0.1f});
    CreateMeshEntity({0, 0, 0}, TitleScreenBackground);
    CreateMeshEntity({0, 0.0, 12.5}, GoalPost);
}

void TitleScreen::Update(float deltaTime)
{
    float deltaSecond = deltaTime / 1000.0f;
    m_CameraController->Update(deltaSecond);
}

void TitleScreen::Render()
{
    if (m_GameState->CurCameraState == StartMenu)
        m_TitleScreenUI->Render();
    else if (m_GameState->CurCameraState == OptionsMenu)
        m_OptionsMenuUI->Render();
}
