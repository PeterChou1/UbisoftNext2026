//---------------------------------------------------------------------------------
// TestEnvironment.h
//---------------------------------------------------------------------------------
//
// Starts the real engine headlessly, the same way the Game / SceneEditor
// programs do (ECS, GameManager, scripts, scenes), and gives tests access to
// the registered scenes. Frames are run with RunFrame, in the ContestAPI order.
//
#pragma once

#include "Entity.h"

class SceneEditorScene;
class ScenePlayer;

namespace TestEnvironment
{
    // Names of the scenes registered in the GameManager
    constexpr const char* EDITOR_SCENE = "SceneEditor";

    /**
     * \brief Initialise the engine once (safe to call repeatedly)
     */
    void Init();

    SceneEditorScene& Editor();
    ScenePlayer& Player();

    /**
     * \brief One frame exactly like the programs run it:
     *        GameManager::Update, GameManager::Render, ECS.FlushECS
     */
    void RunFrame(float deltaMilliseconds = 16.0f);

    void RunFrames(int count, float deltaMilliseconds = 16.0f);
} // namespace TestEnvironment
