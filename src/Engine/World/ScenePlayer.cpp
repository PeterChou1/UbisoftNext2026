#include "ScenePlayer.h"

#include "../Camera.h"
#include "../ECSManager.h"
#include "../GameOptions.h"
#include "../Input.h"
#include "../Lighting.h"
#include "SceneLight.h"
#include "app.h"

extern ECSManager ECS;

void ScenePlayer::Setup()
{
    // The light follows the scene's light object (GameManager, SceneLight.h)
    SceneLighting::Update(*ECS.GetResource<Lighting>(), *ECS.GetResource<GameOptions>());
    ECS.GetResource<Camera>()->SetProjectionPerspective();
}

void ScenePlayer::OnWorldRestored()
{
    // The scene's camera object (or, in older scenes, the settings' camera)
    m_Camera.Reset();
    m_Camera.Update(*ECS.GetResource<Camera>());
}

void ScenePlayer::Update(float deltaTime)
{
    // Scripts may move the camera object
    m_Camera.Update(*ECS.GetResource<Camera>());
    if (Input::WasPressed(App::KEY_ESC) && OnExit)
        OnExit();
}

void ScenePlayer::Render()
{
    App::Print(10.0f, 10.0f, "Esc: menu", 0.6f, 0.6f, 0.6f);
}
