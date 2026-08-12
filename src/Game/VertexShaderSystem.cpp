#include "VertexShaderSystem.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "stdafx.h"

extern ECSManager ECS;

VertexShaderSystem::VertexShaderSystem()
{
    m_VertexBuffer = ECS.GetResource<VertexBuffer>();
    m_cam = ECS.GetResource<Camera>();
    m_Lighting = ECS.GetResource<Lighting>();
    m_Options = ECS.GetResource<GameOptions>();
}

void VertexShaderSystem::Shade()
{
    auto& buffer = m_VertexBuffer->Buffer;
    DirectionalLight& Light = m_Lighting->GetDirectionalLight();
    AssetServer& Server = AssetServer::GetInstance();
    Concurrent::ForEach(buffer.begin(), buffer.end(), [&](Vertex& v) {
        std::shared_ptr<VertexShader> shader = Server.GetVertShader(v.VertexShaderID);
        shader->Shade(v, *m_cam, Light);
    });
}
