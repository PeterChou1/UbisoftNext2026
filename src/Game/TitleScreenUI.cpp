#include "TitleScreenUI.h"

#include "AppSettings.h"
#include "ECSManager.h"
#include "GameManager.h"
#include "Widget.h"
#include "stdafx.h"

extern ECSManager ECS;
extern GameManager GameSceneManager;

TitleScreenUI::TitleScreenUI()
{
    m_state = ECS.GetResource<UIState>();
    m_gamestate = ECS.GetResource<GameState>();
    m_cam = ECS.GetResource<Camera>();
}

void TitleScreenUI::Render()
{
    float MidPointX = APP_VIRTUAL_WIDTH / 2;
    float MidPointY = APP_VIRTUAL_HEIGHT / 2;
    if (Button(1, MidPointX - 100, MidPointY, *m_state, 200, 50, "Start"))
    {
        GameSceneManager.SetActiveScene("MainLevel");
        return;
    }
    if (Button(2, MidPointX - 100, MidPointY - 200, *m_state, 200, 50, "Game Options"))
    {
        Transform& start = m_cam->CamTransform;
        Vec3 NewPos = {0, 3, 10.5};
        Vec3 Forward = m_cam->CamTransform.GetForward();
        Vec3 NewTarget = NewPos - Forward;
        Transform end = Transform(NewPos, NewTarget, {0, 1, 0});
        m_gamestate->LerpToTargetCamera(1.0, start, end, OptionsMenu);
    }
}
