#include "TitleScreenCameraController.h"

#include "ECSManager.h"
#include "Utils.h"
#include "stdafx.h"

extern ECSManager ECS;

TitleScreenCameraController::TitleScreenCameraController()
{
    m_Cam = ECS.GetResource<Camera>();
    m_GameState = ECS.GetResource<GameState>();
}

void TitleScreenCameraController::Update(float deltaTime)
{
    if (m_GameState->CurCameraState == LerpToPosition)
    {
        m_GameState->LerpProgress += deltaTime / m_GameState->LerpTime;
        m_GameState->LerpProgress = Utils::Clamp(m_GameState->LerpProgress, 0, 1);
        Transform camTransform = Transform::Lerp(
                m_GameState->TransformStart, m_GameState->TransformEnd, m_GameState->LerpProgress);
        m_Cam->SetTransform(camTransform);
        if (m_GameState->LerpProgress == 1.0)
            m_GameState->CurCameraState = m_GameState->CameraEndState;
    }
}
