#include "ShaderHandler.h"

#include "AssetServer.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "VertShaderTag.h"
#include "stdafx.h"

extern ECSManager ECS;

ShaderHandler::ShaderHandler()
{
    m_Options = ECS.GetResource<GameOptions>();
    m_Constants = ECS.GetResource<RenderConstants>();
}

void ShaderHandler::Update(float deltaTime)
{
    const float dt = deltaTime / 1000.0f;
    const bool shadows = m_Options->ShadowsOn();
    AssetServer& server = AssetServer::GetInstance();
    auto advance = [&](auto& shader) {
        shader.DeltaTime += dt;
        shader.ShadowMapping = shadows;
    };

    auto& entityToFragType = m_Constants->EntityToFragShaderType;
    for (auto e : ECS.Visit<FragShaderTag>())
    {
        FragShaderTag& shader = ECS.GetComponent<FragShaderTag>(e);
        // A new instance when the tag is new or its shader type changed
        bool typeChanged =
                entityToFragType.count(e) > 0 && entityToFragType[e] != shader.FragAssetId;
        if (!shader.Initialized || typeChanged)
        {
            server.SetFragShader(e, shader);
            entityToFragType[e] = shader.FragAssetId;
            shader.Initialized = true;
        }
        advance(*server.GetFragShader(shader.FragShaderID));
    }

    auto& entityToVertType = m_Constants->EntityToVertShaderType;
    for (auto e : ECS.Visit<VertShaderTag>())
    {
        VertShaderTag& shader = ECS.GetComponent<VertShaderTag>(e);
        bool typeChanged =
                entityToVertType.count(e) > 0 && entityToVertType[e] != shader.VertAssetId;
        if (!shader.Initialized || typeChanged)
        {
            server.SetVertShader(e, shader);
            entityToVertType[e] = shader.VertAssetId;
            shader.Initialized = true;
        }
        advance(*server.GetVertShader(shader.VertShaderID));
    }

    advance(*AssetServer::DefaultFragShader);
    advance(*AssetServer::DefaultVertShader);
}

void ShaderHandler::HandleShaderDelete()
{
    auto& entityToFragType = m_Constants->EntityToFragShaderType;
    auto& entityToVertType = m_Constants->EntityToVertShaderType;
    AssetServer& server = AssetServer::GetInstance();
    // An entity can be destroyed before its shader was ever initialized
    // (e.g. created and deleted in the same frame by the scene editor)
    for (const auto e : ECS.VisitDeleted<FragShaderTag>())
    {
        if (entityToFragType.erase(e) > 0)
            server.RemoveFragShader(e);
    }
    for (const auto e : ECS.VisitDeleted<VertShaderTag>())
    {
        if (entityToVertType.erase(e) > 0)
            server.RemoveVertShader(e);
    }
}
