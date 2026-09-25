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
#include "RenderConstants.h"
#include "Transform.h"
#include "VertexBuffer.h"

#include <memory>
#include <string>
#include <vector>

class MeshHandler
{
  public:
    MeshHandler();

    void Update();

    /**
     * \brief Remove the geometry of entities destroyed this frame. Also run
     *        at the end of GameManager::Render: objects destroyed while
     *        rendering (editor buttons, script OnRender) are flushed from the
     *        ECS before the next Update could see them
     */
    void DeleteDestroyed();

  private:
    // Triangle count and triangles per core, after the index buffer changed
    void UpdateCoreInterval();

    // Append geometry (in world space) to the vertex / index buffers
    void AddMesh(Entity entity,
                 std::vector<Vertex> vertices,
                 const std::vector<std::uint32_t>& indices,
                 size_t fragShaderID,
                 size_t vertShaderID);

    void AddModel(Entity entity,
                  const std::string& model,
                  Transform& transform,
                  size_t fragShaderID,
                  size_t vertShaderID);

    void DeleteMeshes(const std::vector<Entity>& entities);

    void UpdateMeshTransform(Entity entity, Transform& transform);

    // Give the entity's vertices its current shader instances
    void UpdateMeshShaders(Entity entity, size_t fragShaderID, size_t vertShaderID);

    std::shared_ptr<IndexBuffer> m_IndexBuffer;
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    std::shared_ptr<RenderConstants> m_RenderConstants;
    std::shared_ptr<Camera> m_Cam;
};
