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
#include "Tiles.h"
#include "VertexBuffer.h"
#include "stdafx.h"

// Initialized at the start of the Game
extern ECSManager ECS;
// User defined function to Register Scenes

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
    assert(m_SceneMap.count(m_ActiveScene) > 0 && "Active Scene Name Not registered");
    // ECS.GetResource<SpriteManager>()->Update(deltaTime);
    // m_DebugCamera->Update(deltaTime);
    // m_DebugMesh->Update(deltaTime);
    m_PhysicsSystem->Update(deltaTime);
    m_ParticleSystem->Update(deltaTime);
    // m_DebugPhysicsRender->Update(deltaTime);
    // Update the UI State
    m_UIStateManager->Update();
    m_SceneMap[m_ActiveScene]->Update(deltaTime);
    m_ShaderHandler->Update(deltaTime);
    m_MeshHandler->Update();
    m_BlackBoardSync->Update(deltaTime);
    m_AISystem->Update();
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
    ECS.Reset();
    m_ActiveScene = sceneName;
    assert(m_SceneMap.count(sceneName) > 0 && "Scene name does not exist");
    m_SceneMap[m_ActiveScene]->Setup();
}
