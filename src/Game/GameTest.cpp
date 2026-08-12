//------------------------------------------------------------------------
// Game.cpp
//------------------------------------------------------------------------
#include "stdafx.h"
//------------------------------------------------------------------------
#include "ECSManager.h"
#include "GameManager.h"
#include "MainLevel.h"
#include "TitleScreen.h"
#include "WinScreen.h"

#include <memory>
//------------------------------------------------------------------------

ECSManager ECS;
GameManager GameSceneManager;

//------------------------------------------------------------------------
// Called before first update. Do any initial setup here.
//------------------------------------------------------------------------
void Init()
{
    // These must be initialized before any other systems
    ECS.Init();
    GameSceneManager.Setup();
    // Register all game scenes below
    std::unique_ptr<Scene> titleScreen = std::make_unique<TitleScreen>();
    std::unique_ptr<Scene> mainLevel = std::make_unique<MainLevel>();
    std::unique_ptr<Scene> winScreen = std::make_unique<WinScreen>();
    GameSceneManager.RegisterScene("MainLevel", std::move(mainLevel));
    GameSceneManager.RegisterScene("TitleScreen", std::move(titleScreen));
    GameSceneManager.RegisterScene("WinScreen", std::move(winScreen));
    GameSceneManager.SetActiveScene("TitleScreen");
}

//------------------------------------------------------------------------
// Update your simulation here. deltaTime is the elapsed time since the last
// update in ms. This will be called at no greater frequency than the value of
// APP_MAX_FRAME_RATE
//------------------------------------------------------------------------
void Update(float deltaTime)
{
    GameSceneManager.Update(deltaTime);
}

//------------------------------------------------------------------------
// Add your display calls here (DrawLine,Print, DrawSprite.)
// See App.h
//------------------------------------------------------------------------
void Render()
{
    GameSceneManager.Render();
    ECS.FlushECS();
}

//------------------------------------------------------------------------
// Add your shutdown code here. Called when the APP_QUIT_KEY is pressed.
// Just before the app exits.
//------------------------------------------------------------------------
void Shutdown() {}
