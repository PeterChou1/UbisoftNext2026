#include "GameManager.h"

#include "AppSettings.h"
#include "BlackBoard.h"
#include "ClippedTriangleBuffer.h"
#include "ColliderCallbackSystem.h"
#include "DepthBuffer.h"
#include "ECSManager.h"
#include "GameOptions.h"
#include "GameState.h"
#include "IndexBuffer.h"
#include "Lighting.h"
#include "PixelBuffer.h"
#include "RenderConstants.h"
#include "Serialization/GameSerialization.h"
#include "Serialization/WorldSerializer.h"
#include "Tiles.h"
#include "VertexBuffer.h"
#include "app.h"
#include "stdafx.h"

// Initialized at the start of the Game
extern ECSManager ECS;
// User defined function to Register Scenes

namespace
{
    // Metadata key storing the scene a save belongs to
    constexpr const char* SAVE_SCENE_KEY = "Scene";
    // How long save / load messages stay on screen (ms)
    constexpr float STATUS_DISPLAY_TIME = 2500.0f;
} // namespace

void GameManager::Setup()
{
    // Register Resource required for GameOptions
    ECS.RegisterResource(GameOptions());
    // ECS.RegisterResource(GameState());
    // Register Resources Required For Rendering
    ECS.RegisterResource(Camera());
    ECS.RegisterResource(VertexBuffer());
    ECS.RegisterResource(IndexBuffer());
    ECS.RegisterResource(ClippedTriangleBuffer());
    ECS.RegisterResource(
            Tiles(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT, APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT));
    ECS.RegisterResource(DepthBuffer(
            APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT, APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT));
    ECS.RegisterResource(PixelBuffer(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT));
    ECS.RegisterResource(ColorBuffer(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT));
    ECS.RegisterResource(RenderConstants());
    ECS.RegisterResource(Lighting());
    // Register Resource required for UI
    ECS.RegisterResource(UIState());
    // Register Resource required for physics
    ECS.RegisterResource(ColliderCallbackSystem());
    // Register Resource required for AI Systems
    ECS.RegisterResource(BlackBoard());
    ECS.RegisterResource(GameState());
    // ECS.RegisterResource(SpriteManager());
    // Initialize Common Systems
    m_VertexShader = std::make_unique<VertexShaderSystem>();
    m_Clipper = std::make_unique<ClipperSystem>();
    m_Rasterizer = std::make_unique<RasterizerSystem>();
    m_FragmentShader = std::make_unique<FragmentShaderSystem>();
    m_MeshHandler = std::make_unique<MeshHandler>();
    m_UIStateManager = std::make_unique<UIStateManager>();
    m_PhysicsSystem = std::make_unique<PhysicsSystem>();
    m_BlackBoardSync = std::make_unique<BlackBoardSystem>();
    m_AISystem = std::make_unique<AISystem>();
    m_ParticleSystem = std::make_unique<ParticleSystem>();
    m_ShaderHandler = std::make_unique<ShaderHandler>();
    // m_TimerSystem = std::make_unique<TimerSystem>();
    // Debug Systems
    m_DebugCamera = std::make_unique<DebugCamera>();
    m_DebugMesh = std::make_unique<DebugMesh>();
    m_DebugPhysicsRender = std::make_unique<DebugPhysicsRenderer>();
}

void GameManager::Update(float deltaTime)
{
    // Save / load between frames, before any system touches the world
    ProcessSaveRequests();
    if (m_StatusTimer > 0.0f)
        m_StatusTimer -= deltaTime;

    assert(m_SceneMap.count(m_ActiveScene) > 0 && "Active Scene Name Not registered");
    // ECS.GetResource<SpriteManager>()->Update(deltaTime);
    // m_DebugCamera->Update(deltaTime);
    // m_DebugMesh->Update(deltaTime);
    bool simulate = m_SceneMap[m_ActiveScene]->SimulatesWorld();
    if (simulate)
    {
        m_PhysicsSystem->Update(deltaTime);
        m_ParticleSystem->Update(deltaTime);
    }
    // m_DebugPhysicsRender->Update(deltaTime);
    // Update the UI State
    m_UIStateManager->Update();
    m_SceneMap[m_ActiveScene]->Update(deltaTime);
    m_ShaderHandler->Update(deltaTime);
    m_MeshHandler->Update();
    if (simulate)
    {
        m_BlackBoardSync->Update(deltaTime);
        m_AISystem->Update();
    }
}

void GameManager::Render()
{
    assert(m_SceneMap.count(m_ActiveScene) > 0 && "Active Scene Name Not registered");

    // Render Pipeline
    m_VertexShader->Shade();
    m_Clipper->Clip();
    m_Rasterizer->Rasterize();
    m_FragmentShader->Shade();
    // Debug Stuff
    // m_DebugCamera->Render();
    // m_DebugPhysicsRender->Render();
    m_SceneMap[m_ActiveScene]->Render();
    if (m_StatusTimer > 0.0f)
        App::Print(20.0f, APP_VIRTUAL_HEIGHT - 40.0f, m_StatusMessage.c_str(), 1.0f, 1.0f, 0.0f);
    // Debug AI System
    // m_BlackBoardSync->Render();
    // Clear Render Pipeline to get ready for next render pass
    m_ShaderHandler->HandleShaderDelete();
    ECS.GetResource<ClippedTriangleBuffer>()->ResetResource();
    ECS.GetResource<Tiles>()->ResetResource();
    ECS.GetResource<DepthBuffer>()->ResetResource();
    ECS.GetResource<PixelBuffer>()->ResetResource();
    ECS.GetResource<ColorBuffer>()->ResetResource();
    m_UIStateManager->CleanUp();
}

void GameManager::RegisterScene(const std::string& sceneName, std::unique_ptr<Scene> scene)
{
    m_SceneMap[sceneName] = std::move(scene);
    m_SceneMap[sceneName]->Start();
}

void GameManager::SetActiveScene(const std::string& sceneName)
{
    // A play test only lasts while its scene (the main level) is running
    if (sceneName != "MainLevel")
        m_PlaytestReturnScene.clear();
    ECS.Reset();
    m_ActiveScene = sceneName;
    assert(m_SceneMap.count(sceneName) > 0 && "Scene name does not exist");
    m_SceneMap[m_ActiveScene]->Setup();
}

bool GameManager::SaveGame(const std::string& path, std::string& error)
{
    Serialization::WorldSerializer serializer(Serialization::GetGameSerializationRegistry());
    Serialization::SaveResult result =
            serializer.SaveToFile(ECS, path, {{SAVE_SCENE_KEY, m_ActiveScene}});
    if (!result)
        error = result.Error;
    return result.Success;
}

bool GameManager::LoadGame(const std::string& path, std::string& error)
{
    Serialization::WorldSerializer serializer(Serialization::GetGameSerializationRegistry());

    // 1. Read + validate everything before touching the running game
    std::vector<std::uint8_t> bytes;
    if (!Serialization::WorldSerializer::ReadFile(path, bytes, error))
        return false;
    Serialization::WorldSnapshot snapshot;
    Serialization::LoadResult parsed = serializer.Parse(bytes, snapshot);
    if (!parsed)
    {
        error = parsed.Error;
        return false;
    }
    auto sceneEntry = parsed.Metadata.find(SAVE_SCENE_KEY);
    if (sceneEntry == parsed.Metadata.end() || m_SceneMap.count(sceneEntry->second) == 0)
    {
        error = "Save file does not belong to a known scene";
        return false;
    }

    // 2. Set the scene up exactly like a normal scene switch (assets, lights,
    //    camera, collision callbacks, systems ...)
    SetActiveScene(sceneEntry->second);

    // 3. Replace the freshly created world with the saved one
    serializer.Apply(ECS, snapshot);

    // 4. Rebuild runtime only state (behaviour trees, AI grids ...)
    m_SceneMap[m_ActiveScene]->OnWorldRestored();
    return true;
}

void GameManager::RequestSave(const std::string& path)
{
    m_PendingSave = path;
}

void GameManager::RequestLoad(const std::string& path)
{
    m_PendingLoad = path;
}

void GameManager::BeginPlaytest(const std::string& path, const std::string& returnScene)
{
    m_PlaytestReturnScene = returnScene;
    RequestLoad(path);
}

bool GameManager::EndPlaytest()
{
    if (m_PlaytestReturnScene.empty())
        return false;
    m_PendingSceneSwitch = m_PlaytestReturnScene;
    return true;
}

void GameManager::ProcessSaveRequests()
{
    if (!m_PendingSceneSwitch.empty())
    {
        std::string scene = m_PendingSceneSwitch;
        m_PendingSceneSwitch.clear();
        SetActiveScene(scene);
    }

    std::string error;
    if (!m_PendingSave.empty())
    {
        std::string path = m_PendingSave;
        m_PendingSave.clear();
        ShowStatus(SaveGame(path, error) ? "Game saved" : "Save failed: " + error);
    }
    if (!m_PendingLoad.empty())
    {
        std::string path = m_PendingLoad;
        m_PendingLoad.clear();
        if (!LoadGame(path, error))
        {
            m_PlaytestReturnScene.clear();
            ShowStatus("Load failed: " + error);
        }
        else if (IsPlaytesting())
            ShowStatus("Play test - press TAB to return to the editor");
        else
            ShowStatus("Game loaded");
    }
}

void GameManager::ShowStatus(const std::string& message)
{
    m_StatusMessage = message;
    m_StatusTimer = STATUS_DISPLAY_TIME;
}
