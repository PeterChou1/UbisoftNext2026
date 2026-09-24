//---------------------------------------------------------------------------------
// GameManager.h
//---------------------------------------------------------------------------------
//
// Manages All Scenes in the Game as well as Systems/Resources
// Common to all scenes such as the Render and Physics System
//
#pragma once

#include "AISystem.h"
#include "BlackBoardSystem.h"
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
#include "ShaderHandler.h"
#include "UIStateManager.h"
#include "VertexShaderSystem.h"
#include "stdafx.h"

#include <memory>

class GameManager
{
  public:
    /**
     * \brief  Set up the game instance as well as all scenes in the game
     */
    void Setup();

    /**
     * \brief Update loop runs current active scene as well as physics loop
     * \param deltaTime
     */
    void Update(float deltaTime);

    /**
     * \brief Runs the Render pipeline
     */
    void Render();

    /**
     * \brief Registers a scene within the game
     */
    void RegisterScene(const std::string& sceneName, std::unique_ptr<Scene> scene);

    /**
     * \brief Resets the ECS and sets a new current scene
     * \param sceneName
     */
    void SetActiveScene(const std::string& sceneName);

    const std::string& GetActiveScene() const { return m_ActiveScene; }

    // ---------------------------------------------------------------------
    // Save system
    // ---------------------------------------------------------------------

    // Default slot used by the quick save / quick load keys
    static constexpr const char* QUICK_SAVE_PATH = "saves/quicksave.ubsave";

    /**
     * \brief Save the current scene and complete world state to a file
     * \return true on success, otherwise error describes the problem
     */
    bool SaveGame(const std::string& path, std::string& error);

    /**
     * \brief Load a save file: switches to the saved scene, replaces its world
     *        with the saved one and lets the scene rebuild its runtime state.
     *        The file is fully validated first, a bad file leaves the game as is
     * \return true on success, otherwise error describes the problem
     */
    bool LoadGame(const std::string& path, std::string& error);

    /**
     * \brief Queue a save / load. Requests are processed at the start of the
     *        next Update so they never happen in the middle of a frame (safe to
     *        call from inside scene / system code)
     */
    void RequestSave(const std::string& path = QUICK_SAVE_PATH);

    void RequestLoad(const std::string& path = QUICK_SAVE_PATH);

  private:
    void ProcessSaveRequests();

    void ShowStatus(const std::string& message);

    std::string m_PendingSave;
    std::string m_PendingLoad;
    std::string m_StatusMessage;
    float m_StatusTimer = 0.0f;
    std::string m_ActiveScene;
    std::unordered_map<std::string, std::unique_ptr<Scene>> m_SceneMap;
    std::unique_ptr<ParticleSystem> m_ParticleSystem;
    std::unique_ptr<BlackBoardSystem> m_BlackBoardSync;
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
    std::unique_ptr<AISystem> m_AISystem;
    std::unique_ptr<ShaderHandler> m_ShaderHandler;
    // std::unique_ptr<TimerSystem> m_TimerSystem;
};