#include "ScenePlayer.h"

#include "../Camera.h"
#include "../ECSManager.h"
#include "../GameOptions.h"
#include "../Input.h"
#include "../Lighting.h"
#include "SceneComponents.h"
#include "SceneObjects.h"
#include "app.h"

extern ECSManager ECS;

void ScenePlayer::Setup()
{
    auto light = ECS.GetResource<Lighting>();
    light->SetLightPerspective(120.0f, ECS.GetResource<GameOptions>()->ScreenRatio, 0.1f, 1000.0f);
    light->SetPositionAndTarget(Vec3(0.0f, 25.0f, -5.0f), Vec3(0.0f, 0.0f, 0.0f));
    ECS.GetResource<Camera>()->SetProjectionPerspective();
}

void ScenePlayer::OnWorldRestored()
{
    // Camera configured in the editor's scene settings
    auto settings = ECS.GetResource<SceneSettings>();
    SceneObjects::ApplyCamera(*ECS.GetResource<Camera>(), settings->CameraTarget, settings->CameraDistance);
}

void ScenePlayer::Update(float deltaTime)
{
    if (Input::WasPressed(App::KEY_ESC) && OnExit)
        OnExit();
}

void ScenePlayer::Render()
{
    App::Print(10.0f, 10.0f, "Esc: menu", 0.6f, 0.6f, 0.6f);
}
