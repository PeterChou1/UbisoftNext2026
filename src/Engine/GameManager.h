//---------------------------------------------------------------------------------
// GameManager.h
//---------------------------------------------------------------------------------
//
// Hosts the engine: the ECS resources and systems common to every scene
// (rendering, physics, particles, UI, scripting) and the registered scenes.
//
// Each frame (Update):
//   save / load / restart requests -> input -> physics -> particles
//   -> scripts -> UI -> active scene -> shaders -> meshes
// Physics, particles and scripts only run while the active scene
// SimulatesWorld() (an editor keeps its world frozen until it plays).
//
// Scene files (see Serialization/WorldSerializer.h) name the Scene that plays
// them in their metadata; LoadGame switches to that scene and restores the
// world into it.
//
#pragma once

#include "ClipperSystem.h"
#include "DebugCamera.h"
#include "DebugMesh.h"
#include "DebugPhysicsRenderer.h"
#include "FragmentShaderSystem.h"
#include "MeshHandler.h"
#include "ParticleSystem.h"
#include "PhysicsSystem.h"
#include "RasterizerSystem.h"
#include "Scene.h"
#include "Scripting/ScriptSystem.h"
#include "ShaderHandler.h"
#include "UIStateManager.h"
#include "VertexShaderSystem.h"
#include "stdafx.h"

#include <memory>
#include <string>
#include <unordered_map>

class GameManager
{
  public:
    // Metadata key naming the Scene that plays a scene file
    static constexpr const char* SCENE_KEY = "Scene";
    static constexpr const char* SCENES_DIRECTORY = "data/scenes";
    static constexpr const char* SCENE_EXTENSION = ".ubsave";

    /**
     * \brief Register the engine resources and create the common systems
     */
    void Setup();

    void Update(float deltaTime);

    void Render();

    void RegisterScene(const std::string& sceneName, std::unique_ptr<Scene> scene);

    /**
     * \brief Clear the ECS (Reset) and set up another scene
     */
    void SetActiveScene(const std::string& sceneName);

    const std::string& GetActiveScene() const { return m_ActiveScene; }

    bool HasScene(const std::string& sceneName) const { return m_SceneMap.count(sceneName) > 0; }

    ScriptSystem& Scripts() { return *m_ScriptSystem; }

    // ---------------------------------------------------------------------
    // Scene files
    // ---------------------------------------------------------------------

    /**
     * \brief data/scenes/<name>.ubsave
     */
    static std::string ScenePath(const std::string& sceneName);

    /**
     * \brief Save the active scene's world to a file
     */
    bool SaveGame(const std::string& path, std::string& error);

    /**
     * \brief Load a scene file: switches to the Scene named in the file, replaces
     *        its world and restarts the scripts. The file is fully validated
     *        first, a bad file leaves the game as is
     */
    bool LoadGame(const std::string& path, std::string& error);

    /**
     * \brief Queue a save / load / restart, processed at the start of the next
     *        Update so it never happens in the middle of a frame
     */
    void RequestSave(const std::string& path);
    void RequestLoad(const std::string& path);
    void RequestRestart();

    /**
     * \brief Switch scene at the start of the next frame (safe to call from
     *        inside a scene's or script's update)
     */
    void RequestSceneChange(const std::string& sceneName);

    /**
     * \brief File most recently loaded with LoadGame (empty if none)
     */
    const std::string& CurrentScenePath() const { return m_CurrentScenePath; }

    /**
     * \brief Drop every per entity render cache. Needed after the world was
     *        replaced without per entity delete events (ECS::ClearWorld)
     */
    void ResetRenderCaches();

    /**
     * \brief Short message drawn on screen for a few seconds
     */
    void ShowStatus(const std::string& message);

  private:
    void ProcessRequests();

    std::string m_ActiveScene;
    std::string m_CurrentScenePath;
    std::string m_PendingSave;
    std::string m_PendingLoad;
    std::string m_PendingScene;
    std::string m_StatusMessage;
    float m_StatusTimer = 0.0f;
    std::unordered_map<std::string, std::unique_ptr<Scene>> m_SceneMap;
    std::unique_ptr<ParticleSystem> m_ParticleSystem;
    std::unique_ptr<UIStateManager> m_UIStateManager;
    std::unique_ptr<VertexShaderSystem> m_VertexShader;
    std::unique_ptr<ClipperSystem> m_Clipper;
    std::unique_ptr<RasterizerSystem> m_Rasterizer;
    std::unique_ptr<FragmentShaderSystem> m_FragmentShader;
    std::unique_ptr<MeshHandler> m_MeshHandler;
    std::unique_ptr<DebugCamera> m_DebugCamera;
    std::unique_ptr<DebugMesh> m_DebugMesh;
    std::unique_ptr<PhysicsSystem> m_PhysicsSystem;
    std::unique_ptr<DebugPhysicsRenderer> m_DebugPhysicsRender;
    std::unique_ptr<ShaderHandler> m_ShaderHandler;
    std::unique_ptr<ScriptSystem> m_ScriptSystem;
};
