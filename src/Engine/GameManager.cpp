#include "GameManager.h"

#include "AppSettings.h"
#include "ClippedTriangleBuffer.h"
#include "ColliderCallbackSystem.h"
#include "DepthBuffer.h"
#include "ECSManager.h"
#include "GameOptions.h"
#include "IndexBuffer.h"
#include "Input.h"
#include "Lighting.h"
#include "Log.h"
#include "PixelBuffer.h"
#include "RenderConstants.h"
#include "Serialization/SceneSerialization.h"
#include "Serialization/WorldSerializer.h"
#include "Tiles.h"
#include "VertexBuffer.h"
#include "World/SceneComponents.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

namespace
{
    // How long status messages stay on screen (ms)
    constexpr float STATUS_DISPLAY_TIME = 2500.0f;

    Serialization::WorldSerializer Serializer()
    {
        return Serialization::WorldSerializer(Serialization::GetSceneSerializationRegistry());
    }
} // namespace

void GameManager::Setup()
{
    ECS.RegisterResource(GameOptions());
    // Rendering
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
    // UI
    ECS.RegisterResource(UIState());
    // Physics
    ECS.RegisterResource(ColliderCallbackSystem());
    // Authored scenes
    ECS.RegisterResource(SceneSettings());

    // Common systems
    m_VertexShader = std::make_unique<VertexShaderSystem>();
    m_Clipper = std::make_unique<ClipperSystem>();
    m_Rasterizer = std::make_unique<RasterizerSystem>();
    m_FragmentShader = std::make_unique<FragmentShaderSystem>();
    m_MeshHandler = std::make_unique<MeshHandler>();
    m_UIStateManager = std::make_unique<UIStateManager>();
    m_PhysicsSystem = std::make_unique<PhysicsSystem>();
    m_ParticleSystem = std::make_unique<ParticleSystem>();
    m_ShaderHandler = std::make_unique<ShaderHandler>();
    m_ScriptSystem = std::make_unique<ScriptSystem>();
    // Debug systems
    m_DebugCamera = std::make_unique<DebugCamera>();
    m_DebugMesh = std::make_unique<DebugMesh>();
    m_DebugPhysicsRender = std::make_unique<DebugPhysicsRenderer>();
}

void GameManager::Update(float deltaTime)
{
    // Save / load between frames, before any system touches the world
    ProcessRequests();
    if (m_StatusTimer > 0.0f)
        m_StatusTimer -= deltaTime;

    assert(m_SceneMap.count(m_ActiveScene) > 0 && "Active Scene Name Not registered");
    Input::Update();
    // Mouse / click state first: scripts and scenes see this frame's clicks
    m_UIStateManager->Update();
    // Tab switches between the two renderers: hardware triangles (fast,
    // default) and the engine's software rasterizer (lit, shadows)
    if (Input::WasPressed(App::KEY_TAB))
    {
        auto options = ECS.GetResource<GameOptions>();
        options->LineRendering = !options->LineRendering;
        ShowStatus(options->LineRendering ? "Renderer: hardware triangles (Tab)"
                                          : "Renderer: software rasterizer (Tab)");
    }
    bool simulate = m_SceneMap[m_ActiveScene]->SimulatesWorld();
    if (simulate)
    {
        m_PhysicsSystem->Update(deltaTime);
        m_ParticleSystem->Update(deltaTime);
        m_ScriptSystem->Update(deltaTime);
    }
    m_SceneMap[m_ActiveScene]->Update(deltaTime);
    m_ShaderHandler->Update(deltaTime);
    m_MeshHandler->Update();
}

void GameManager::Render()
{
    assert(m_SceneMap.count(m_ActiveScene) > 0 && "Active Scene Name Not registered");

    // Render Pipeline
    m_VertexShader->Shade();
    m_Clipper->Clip();
    m_Rasterizer->Rasterize();
    m_FragmentShader->Shade();
    // Script HUDs, then the scene's own UI on top
    if (m_SceneMap[m_ActiveScene]->SimulatesWorld())
        m_ScriptSystem->Render();
    m_SceneMap[m_ActiveScene]->Render();
    if (m_StatusTimer > 0.0f)
        App::Print(20.0f, APP_VIRTUAL_HEIGHT - 40.0f, m_StatusMessage.c_str(), 1.0f, 1.0f, 0.0f);
    // Clear Render Pipeline to get ready for next render pass
    m_MeshHandler->DeleteDestroyed();
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
    assert(m_SceneMap.count(sceneName) > 0 && "Scene name does not exist");
    // Scripts refer to entities of the old world
    m_ScriptSystem->Reset();
    Input::Reset();
    ECS.Reset();
    // A fresh world does not come from a file (LoadGame sets it afterwards),
    // restarting must not reload a previous scene
    m_CurrentScenePath.clear();
    m_ActiveScene = sceneName;
    LOG_INFO("Scene", "Active scene: %s", sceneName.c_str());
    m_SceneMap[m_ActiveScene]->Setup();
}

std::string GameManager::ScenePath(const std::string& sceneName)
{
    return std::string(SCENES_DIRECTORY) + "/" + sceneName + SCENE_EXTENSION;
}

bool GameManager::SaveGame(const std::string& path, std::string& error)
{
    Serialization::SaveResult result = Serializer().SaveToFile(ECS, path, {{SCENE_KEY, m_ActiveScene}});
    if (!result)
    {
        error = result.Error;
        LOG_ERROR("Save", "Could not save %s: %s", path.c_str(), error.c_str());
    }
    else
        LOG_INFO("Save", "Saved %s (%zu bytes)", path.c_str(), result.BytesWritten);
    return result.Success;
}

bool GameManager::LoadGame(const std::string& path, std::string& error)
{
    auto fail = [&](const std::string& reason) {
        error = reason;
        LOG_ERROR("Load", "Could not load %s: %s", path.c_str(), reason.c_str());
        return false;
    };
    // 1. Read + validate everything before touching the running game
    std::vector<std::uint8_t> bytes;
    std::string readError;
    if (!Serialization::WorldSerializer::ReadFile(path, bytes, readError))
        return fail(readError);
    Serialization::WorldSnapshot snapshot;
    Serialization::LoadResult parsed = Serializer().Parse(bytes, snapshot);
    if (!parsed)
        return fail(parsed.Error);
    auto sceneEntry = parsed.Metadata.find(SCENE_KEY);
    if (sceneEntry == parsed.Metadata.end() || m_SceneMap.count(sceneEntry->second) == 0)
        return fail("The file does not belong to a scene of this program");

    // 2. Set the scene up like a normal scene switch (resets the ECS)
    SetActiveScene(sceneEntry->second);

    // 3. Replace its world with the saved one
    std::vector<std::string> warnings = Serializer().Apply(ECS, snapshot);
    for (const std::string& warning : parsed.Warnings)
        LOG_WARN("Load", "%s: %s", path.c_str(), warning.c_str());
    for (const std::string& warning : warnings)
        LOG_WARN("Load", "%s: %s", path.c_str(), warning.c_str());
    m_CurrentScenePath = path;
    LOG_INFO("Load", "Loaded %s (%zu entities)", path.c_str(), snapshot.LivingEntities.size());

    // 4. Let the scene react (camera, runtime state); scripts start next frame
    m_SceneMap[m_ActiveScene]->OnWorldRestored();
    m_ScriptSystem->Reset();
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

void GameManager::RequestRestart()
{
    if (!m_CurrentScenePath.empty())
        m_PendingLoad = m_CurrentScenePath;
}

void GameManager::RequestSceneChange(const std::string& sceneName)
{
    m_PendingScene = sceneName;
}

void GameManager::ResetRenderCaches()
{
    ECS.GetResource<VertexBuffer>()->ResetResource();
    ECS.GetResource<IndexBuffer>()->ResetResource();
    auto constants = ECS.GetResource<RenderConstants>();
    constants->ResetResource();
    constants->EntityToFragShaderID.clear();
    constants->EntityToVertShaderID.clear();
    constants->EntityToFragShaderType.clear();
    constants->EntityToVertShaderType.clear();
}

void GameManager::ProcessRequests()
{
    if (!m_PendingScene.empty())
    {
        std::string scene = m_PendingScene;
        m_PendingScene.clear();
        SetActiveScene(scene);
    }

    std::string error;
    if (!m_PendingSave.empty())
    {
        std::string path = m_PendingSave;
        m_PendingSave.clear();
        ShowStatus(SaveGame(path, error) ? "Saved " + path : "Save failed: " + error);
    }
    if (!m_PendingLoad.empty())
    {
        std::string path = m_PendingLoad;
        m_PendingLoad.clear();
        if (!LoadGame(path, error))
            ShowStatus("Load failed: " + error);
    }
}

void GameManager::ShowStatus(const std::string& message)
{
    m_StatusMessage = message;
    m_StatusTimer = STATUS_DISPLAY_TIME;
}
