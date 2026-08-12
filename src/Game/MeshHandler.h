//---------------------------------------------------------------------------------
// MeshHandler.h
//---------------------------------------------------------------------------------
//
// Handles all meshes information in the rendering system
// responsible for adding deleting meshes/updating shaders for a mesh
//
#pragma once

#include "Camera.h"
#include "IndexBuffer.h"
#include "Mesh.h"
#include "RenderConstants.h"
#include "Transform.h"
#include "VertexBuffer.h"

#include <memory>

class MeshHandler
{
  public:
    MeshHandler();

    void Update();

  private:
    void UpdateCoreInterval();

    void AddMeshCommon(Entity entity,
                       const std::vector<Vertex>& vertices,
                       const std::vector<std::uint32_t>& indices,
                       size_t FragShaderID,
                       size_t VertShaderID);

    void AddMeshRaw(Entity entity,
                    std::vector<Vertex> vertices,
                    std::vector<std::uint32_t> indices,
                    size_t FragShaderID,
                    size_t VertShaderID);

    void DeleteMeshes(const std::vector<Entity>& entities);

    void UpdateMeshTransform(Entity entity, Transform& transform);

    void UpdateMeshFragShader(Entity entity, size_t shaderID);

    void UpdateMeshVertShader(Entity entity, size_t shaderID);

    void AddMesh(Entity entity,
                 Mesh mesh,
                 Transform& transform,
                 size_t FragShaderID,
                 size_t VertShaderID);

    std::shared_ptr<IndexBuffer> m_IndexBuffer;
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    std::shared_ptr<RenderConstants> m_RenderConstants;
    std::shared_ptr<Camera> m_Cam;
};
