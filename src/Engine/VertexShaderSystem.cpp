#include "VertexShaderSystem.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "stdafx.h"

extern ECSManager ECS;

VertexShaderSystem::VertexShaderSystem()
{
    m_VertexBuffer = ECS.GetResource<VertexBuffer>();
    m_Cam = ECS.GetResource<Camera>();
    m_Lighting = ECS.GetResource<Lighting>();
}

void VertexShaderSystem::Shade()
{
    auto& buffer = m_VertexBuffer->Buffer;
    DirectionalLight& light = m_Lighting->GetDirectionalLight();
    AssetServer& server = AssetServer::GetInstance();
    Concurrent::ForEach(buffer.begin(), buffer.end(), [&](Vertex& v) {
        server.GetVertShader(v.VertexShaderID)->Shade(v, *m_Cam, light);
    });
}
