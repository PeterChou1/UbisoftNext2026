//---------------------------------------------------------------------------------
// GameMain.cpp
//---------------------------------------------------------------------------------
//
// Entry points of the Game program (called by the ContestAPI main loop).
// The game shows a menu of the authored scenes (data/scenes) and plays them
// with the engine's ScenePlayer, running the project's C++ scripts.
//
#include "stdafx.h"
//------------------------------------------------------------------------
#include "ECSManager.h"
#include "GameManager.h"
#include "GameScripts.h"
#include "SceneMenu.h"
#include "World/ScenePlayer.h"
#include "app.h"

#include <memory>

extern ECSManager ECS;
extern GameManager GameSceneManager;

void Init()
{
    ECS.Init();
    GameSceneManager.Setup();
    RegisterGameScripts();

    auto player = std::make_unique<ScenePlayer>();
    player->OnExit = [] { GameSceneManager.RequestSceneChange(SceneMenu::NAME); };
    GameSceneManager.RegisterScene(ScenePlayer::NAME, std::move(player));
    GameSceneManager.RegisterScene(SceneMenu::NAME, std::make_unique<SceneMenu>());
    GameSceneManager.SetActiveScene(SceneMenu::NAME);
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
