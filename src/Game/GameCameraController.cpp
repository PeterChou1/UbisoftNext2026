#include "GameCameraController.h"

#include "ECSManager.h"
#include "RigidBody.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

GameCameraController::GameCameraController(float StartX, float StartY)
    : StartX(StartX)
    , StartY(StartY)
{
    m_GameState = ECS.GetResource<GameState>();
    m_Cam = ECS.GetResource<Camera>();
}

void GameCameraController::Update(float deltaTime)
{
    // Rotate the camera speed
    float CameraSpeed = 0.30f;
    Vec3 CamPos = m_Cam->Position;
    Vec3 Forward = {0, 0, 1};
    Vec3 Right = {1, 0, 0};

    if (App::IsKeyPressed(App::KEY_A))
    {
        CamPos.X += CameraSpeed * Right.X;
        CamPos.Z += CameraSpeed * Right.Z;
    }
    if (App::IsKeyPressed(App::KEY_D))
    {
        CamPos.X -= CameraSpeed * Right.X;
        CamPos.Z -= CameraSpeed * Right.Z;
    }

    if (App::IsKeyPressed(App::KEY_W))
    {
        CamPos.X += CameraSpeed * Forward.X;
        CamPos.Z += CameraSpeed * Forward.Z;
    }
    if (App::IsKeyPressed(App::KEY_S))
    {
        CamPos.X -= CameraSpeed * Forward.X;
        CamPos.Z -= CameraSpeed * Forward.Z;
    }

    CamPos.X = Utils::Clamp(CamPos.X, -25.0f, 25.0f);
    CamPos.Z = Utils::Clamp(CamPos.Z, -25.0f, 25.0f);

    m_Cam->SetPosition(CamPos);
}
