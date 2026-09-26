//---------------------------------------------------------------------------------
// EngineGlobals.cpp
//---------------------------------------------------------------------------------
//
// The two engine singletons every program built on the engine shares. Engine
// code reaches them through `extern`. A program (SceneEditor, Game, tests)
// only defines the ContestAPI entry points Init / Update / Render / Shutdown.
//
#include "ECSManager.h"
#include "GameManager.h"

ECSManager ECS;
GameManager GameSceneManager;
