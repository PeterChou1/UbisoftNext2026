#include "ShaderHandler.h"

#include "AssetServer.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "stdafx.h"

extern ECSManager ECS;

ShaderHandler::ShaderHandler()
{
    m_Options = ECS.GetResource<GameOptions>();
    m_Constants = ECS.GetResource<RenderConstants>();
}

void ShaderHandler::Update(float deltaTime)
{
    float dt = deltaTime / 1000.0f;
    AssetServer& Server = AssetServer::GetInstance();

    auto& EntToFragType = m_Constants->EntityToFragShaderType;
    auto& EntToVertType = m_Constants->EntityToVertShaderType;
    for (auto e : ECS.Visit<FragShaderTag>())
    {
        FragShaderTag& shader = ECS.GetComponent<FragShaderTag>(e);
        bool ShaderIDChanged = EntToFragType.count(e) > 0 && EntToFragType[e] != shader.FragAssetId;

        // Set Fragment Shader if its not initialized or
        // The ShaderID changed
        if (!shader.Initialized || ShaderIDChanged)
        {
            Server.SetFragShader(e, shader);
            EntToFragType[e] = shader.FragAssetId;
            shader.Initialized = true;
        }

        std::shared_ptr<FragmentShader> AttachedShader = Server.GetFragShader(shader.FragShaderID);
        AttachedShader->DeltaTime += dt;
        AttachedShader->ShadowMapping = m_Options->ShadowMapping;
    }

    for (auto e : ECS.Visit<VertShaderTag>())
    {
        VertShaderTag& shader = ECS.GetComponent<VertShaderTag>(e);

        bool ShaderIDChanged = EntToVertType.count(e) > 0 && EntToVertType[e] != shader.VertAssetId;

        if (!shader.Initialized || ShaderIDChanged)
        {
            Server.SetVertShader(e, shader);
            EntToVertType[e] = shader.VertAssetId;
            shader.Initialized = true;
        }
        std::shared_ptr<VertexShader> AttachedShader = Server.GetVertShader(shader.VertShaderID);
        AttachedShader->DeltaTime += dt;
        AttachedShader->ShadowMapping = m_Options->ShadowMapping;
    }

    Server.defaultFragShader->DeltaTime += dt;
    Server.defaultFragShader->ShadowMapping = m_Options->ShadowMapping;
    Server.defaultVertShader->DeltaTime += dt;
    Server.defaultVertShader->ShadowMapping = m_Options->ShadowMapping;
}

void ShaderHandler::HandleShaderDelete()
{
    // References: the erase below must update the shared maps, not copies
    auto& EntToFragType = m_Constants->EntityToFragShaderType;
    auto& EntToVertType = m_Constants->EntityToVertShaderType;
    AssetServer& Server = AssetServer::GetInstance();
    for (const auto e : ECS.VisitDeleted<FragShaderTag>())
    {
        // An entity can be destroyed before its shader was ever initialized
        // (e.g. created and deleted in the same frame by the scene editor)
        if (EntToFragType.count(e) == 0)
            continue;
        EntToFragType.erase(e);
        Server.RemoveFragShader(e);
    }

    for (const auto e : ECS.VisitDeleted<VertShaderTag>())
    {
        if (EntToVertType.count(e) == 0)
            continue;
        EntToVertType.erase(e);
        Server.RemoveVertShader(e);
    }
}
