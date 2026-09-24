//---------------------------------------------------------------------------------
// ScenePlayer.h
//---------------------------------------------------------------------------------
//
// The Scene that plays authored scene files. Scene files saved by the scene
// editor name it in their metadata (Scene = "Play"), so
// GameManager::LoadGame(file) switches to it and restores the file's world.
//
// While active, the world simulates: physics, particles and the C++ scripts
// (object scripts + the scene script) run every frame.
//
#pragma once

#include "../Scene.h"

#include <functional>

class ScenePlayer : public Scene
{
  public:
    static constexpr const char* NAME = "Play";

    void Setup() override;
    void OnWorldRestored() override;
    void Update(float deltaTime) override;
    void Render() override;
    bool SimulatesWorld() const override { return true; }

    /**
     * \brief Called when the player presses Esc (e.g. go back to a menu)
     */
    std::function<void()> OnExit;
};
