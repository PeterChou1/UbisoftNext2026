//---------------------------------------------------------------------------------
// EditorMain.cpp
//---------------------------------------------------------------------------------
//
// Entry points of the SceneEditor program (called by the ContestAPI main loop).
// The editor links the project's scripts (GameScripts) so it can offer them in
// the inspector and run them in play mode.
//
#include "stdafx.h"
//------------------------------------------------------------------------
#include "ECSManager.h"
#include "GameManager.h"
#include "GameScripts.h"
#include "SceneEditorScene.h"
#include "app.h"

#include <memory>

extern ECSManager ECS;
extern GameManager GameSceneManager;

void Init()
{
    ECS.Init();
    GameSceneManager.Setup();
    RegisterGameScripts();
    GameSceneManager.RegisterScene("SceneEditor", std::make_unique<SceneEditorScene>());
    GameSceneManager.SetActiveScene("SceneEditor");
}

void Update(float deltaTime)
{
    GameSceneManager.Update(deltaTime);
}

void Render()
{
    GameSceneManager.Render();
    ECS.FlushECS();
}

void Shutdown() {}
