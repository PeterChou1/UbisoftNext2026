#include "MeshHandler.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "Emitter.h"
#include "FragShaderTag.h"
#include "Transform.h"
#include "Utils.h"
#include "World/SceneComponents.h"
#include "World/ShapeGeometry.h"
#include "stdafx.h"

extern ECSManager ECS;

MeshHandler::MeshHandler()
{
    m_RenderConstants = ECS.GetResource<RenderConstants>();
    m_VertexBuffer = ECS.GetResource<VertexBuffer>();
    m_IndexBuffer = ECS.GetResource<IndexBuffer>();
    m_Cam = ECS.GetResource<Camera>();
}

void MeshHandler::Update()
{
    auto& EntityToFragShaderID = m_RenderConstants->EntityToFragShaderID;
    auto& EntityToVertShaderID = m_RenderConstants->EntityToVertShaderID;

    // -- 1. Process Mesh Entities -------------------------------------------
    for (const auto e : ECS.Visit<Transform, Mesh>())
    {
        auto& mesh = ECS.GetComponent<Mesh>(e);
        auto& transform = ECS.GetComponent<Transform>(e);
        Entity parent = transform.Parent;
        // kill orphaned children
        if (parent != NULL_ENTITY && !ECS.HasComponent<Transform>(parent))
        {
            ECS.DestroyEntity(e);
            continue;
        }

        size_t FragShaderID = 0;
        size_t VertShaderID = 0;

        if (ECS.HasComponent<FragShaderTag>(e))
            FragShaderID = ECS.GetComponent<FragShaderTag>(e).FragShaderID;
        if (ECS.HasComponent<VertShaderTag>(e))
            VertShaderID = ECS.GetComponent<VertShaderTag>(e).VertShaderID;

        if (!mesh.Loaded)
        {
            // New, or the model changed: replace any previous geometry
            if (m_RenderConstants->EntityToVertexRange.count(e) != 0)
                DeleteMeshes({e});
            AddMesh(e, mesh, transform, FragShaderID, VertShaderID);
            mesh.Loaded = true;
        }
        else if (transform.IsDirty)
            UpdateMeshTransform(e, transform);

        // Check if Fragment Shader Changed
        if (EntityToFragShaderID.count(e) != 0 && EntityToFragShaderID[e] != FragShaderID)
        {
            EntityToFragShaderID[e] = FragShaderID;
            UpdateMeshFragShader(e, FragShaderID);
        }
        if (EntityToVertShaderID.count(e) != 0 && EntityToVertShaderID[e] != VertShaderID)
        {
            EntityToVertShaderID[e] = VertShaderID;
            UpdateMeshFragShader(e, VertShaderID);
        }
    }

    // -- 1b. Process 2D shapes (procedural meshes) ----------------------------
    for (const auto e : ECS.Visit<Transform, Shape2D>())
    {
        auto& shape = ECS.GetComponent<Shape2D>(e);
        auto& transform = ECS.GetComponent<Transform>(e);

        size_t FragShaderID = 0;
        size_t VertShaderID = 0;
        if (ECS.HasComponent<FragShaderTag>(e))
            FragShaderID = ECS.GetComponent<FragShaderTag>(e).FragShaderID;
        if (ECS.HasComponent<VertShaderTag>(e))
            VertShaderID = ECS.GetComponent<VertShaderTag>(e).VertShaderID;

        if (!shape.Built)
        {
            // (Re)build: the shape is new or was edited (size, colour ...)
            if (m_RenderConstants->EntityToVertexRange.count(e) != 0)
                DeleteMeshes({e});
            ShapeGeometry::MeshData data = ShapeGeometry::BuildMesh(shape);
            AddMeshRaw(e, data.Vertices, data.Indices, FragShaderID, VertShaderID);
            shape.Built = true;
            UpdateMeshTransform(e, transform);
        }
        else if (transform.IsDirty)
            UpdateMeshTransform(e, transform);

        if (EntityToFragShaderID.count(e) != 0 && EntityToFragShaderID[e] != FragShaderID)
        {
            EntityToFragShaderID[e] = FragShaderID;
            UpdateMeshFragShader(e, FragShaderID);
        }
    }

    // -- 2. Process Particle Entities ---------------------------------------
    for (const auto e : ECS.Visit<Transform, Particle, FragShaderTag>())
    {
        auto& particle = ECS.GetComponent<Particle>(e);
        auto& transform = ECS.GetComponent<Transform>(e);
        auto& shader = ECS.GetComponent<FragShaderTag>(e);

        if (!particle.loaded)
        {
            Vec3 CameraRight = m_Cam->CamTransform.GetRight();
            Vec3 CameraUp = m_Cam->CamTransform.GetUp();
            float halfSize = 0.1f;
            Vec3 v0 = CameraRight * -halfSize - CameraUp * halfSize;
            Vec3 v1 = CameraRight * halfSize - CameraUp * halfSize;
            Vec3 v2 = CameraRight * halfSize + CameraUp * halfSize;
            Vec3 v3 = CameraRight * -halfSize + CameraUp * halfSize;

            // Create a Vertex array for the quad which will serve as the particle
            std::vector<Vertex> vertices(4);
            vertices[0].LocalPosition = v0;
            vertices[1].LocalPosition = v1;
            vertices[2].LocalPosition = v2;
            vertices[3].LocalPosition = v3;
            Vec3 lineA = vertices[0].LocalPosition - vertices[1].LocalPosition;
            Vec3 lineB = vertices[0].LocalPosition - vertices[2].LocalPosition;
            Vec3 normal = lineA.Cross(lineB);
            normal.Normalize();
            for (auto& v : vertices)
            {
                v.LocalNormal = normal;
                v.Color = particle.Color;
            }
            std::vector<std::uint32_t> indices = {0, 1, 2, 2, 3, 0};
            AddMeshRaw(e, vertices, indices, shader.FragShaderID, 0);
            particle.loaded = true;
            auto t = AssetServer::GetInstance().GetFragShader(shader.FragShaderID);
        }

        if (transform.IsDirty)
            UpdateMeshTransform(e, transform);
    }
    // -- 3. Delete Entities that lost Particle, Mesh or Shape2D -------------
    DeleteDestroyed();
}

void MeshHandler::DeleteDestroyed()
{
    std::vector<Entity> MarkedForDeletion;
    for (const auto e : ECS.VisitDeleted<Particle>())
        MarkedForDeletion.push_back(e);

    for (const auto e : ECS.VisitDeleted<Mesh>())
        MarkedForDeletion.push_back(e);

    for (const auto e : ECS.VisitDeleted<Shape2D>())
        MarkedForDeletion.push_back(e);

    // DeleteMeshes ignores entities whose geometry is already gone
    if (!MarkedForDeletion.empty())
        DeleteMeshes(MarkedForDeletion);
}

//------------------------------------------------------------------------
//  Common helper to update triangle count and core interval
//------------------------------------------------------------------------
void MeshHandler::UpdateCoreInterval()
{
    m_RenderConstants->TriangleCount = static_cast<uint32_t>(m_IndexBuffer->Buffer.size() / 3);

    unsigned int CoreCount = m_RenderConstants->CoreCount;
    m_RenderConstants->CoreInterval =
            (m_RenderConstants->TriangleCount + CoreCount - 1) / CoreCount;
}

//------------------------------------------------------------------------
//  Core function that both AddMesh and AddMeshRaw can call
//------------------------------------------------------------------------
void MeshHandler::AddMeshCommon(Entity entity,
                                const std::vector<Vertex>& vertices,
                                const std::vector<std::uint32_t>& indices,
                                size_t FragShaderID,
                                size_t VertShaderID)
{
    auto& EntityToVertexRange = m_RenderConstants->EntityToVertexRange;
    auto& EntityToIndexRange = m_RenderConstants->EntityToIndexRange;
    auto& EntityToFragID = m_RenderConstants->EntityToFragShaderID;
    auto& EntityToVertID = m_RenderConstants->EntityToVertShaderID;
    // Record the ranges in the vertex/index buffers
    int offsetVertex = static_cast<int>(m_VertexBuffer->Buffer.size());

    EntityToVertexRange[entity] = {offsetVertex, offsetVertex + static_cast<int>(vertices.size())};

    int offsetIndex = static_cast<int>(m_IndexBuffer->Buffer.size());
    EntityToIndexRange[entity] = {offsetIndex, offsetIndex + static_cast<int>(indices.size())};

    EntityToFragID[entity] = FragShaderID;
    EntityToVertID[entity] = VertShaderID;

    // Append the vertices
    for (auto& v : vertices)
    {
        m_VertexBuffer->Buffer.push_back(v);
    }

    // Append the indices, with the offset
    for (auto id : indices)
    {
        m_IndexBuffer->Buffer.push_back(offsetVertex + id);
    }

    // Update the triangle count and core interval
    UpdateCoreInterval();
}

//------------------------------------------------------------------------
//  Add a raw mesh billboard, etc.
//------------------------------------------------------------------------
void MeshHandler::AddMeshRaw(Entity entity,
                             std::vector<Vertex> vertices,
                             std::vector<std::uint32_t> indices,
                             size_t FragShaderID,
                             size_t VertShaderID)
{
    // Ensure each vertex has the correct shader ID
    for (auto& vertex : vertices)
    {
        vertex.FragShaderID = FragShaderID;
        vertex.VertexShaderID = VertShaderID;
    }

    AddMeshCommon(entity, vertices, indices, FragShaderID, VertShaderID);
    UpdateCoreInterval();
}

//------------------------------------------------------------------------
//  Delete any meshes that no longer need to exist
//------------------------------------------------------------------------
void MeshHandler::DeleteMeshes(const std::vector<Entity>& entities)
{
    std::vector<BufferRange> vertexRangesToRemove;
    std::vector<BufferRange> indexRangesToRemove;
    auto& EntityToVertexRange = m_RenderConstants->EntityToVertexRange;
    auto& EntityToIndexRange = m_RenderConstants->EntityToIndexRange;
    auto& EntityToFragShaderID = m_RenderConstants->EntityToFragShaderID;
    auto& EntityToVertShaderID = m_RenderConstants->EntityToVertShaderID;

    for (const auto& entity : entities)
    {
        if (EntityToVertexRange.count(entity))
        {
            vertexRangesToRemove.push_back(EntityToVertexRange[entity]);
            EntityToVertexRange.erase(entity);
        }
        if (EntityToIndexRange.count(entity))
        {
            indexRangesToRemove.push_back(EntityToIndexRange[entity]);
            EntityToIndexRange.erase(entity);
        }
        if (EntityToFragShaderID.count(entity))
        {
            EntityToFragShaderID.erase(entity);
        }
        if (EntityToVertShaderID.count(entity))
        {
            EntityToVertShaderID.erase(entity);
        }
    }

    // Step 2: Sort ranges to facilitate efficient removal
    auto rangeComparer = [](const BufferRange& a, const BufferRange& b) {
        return a.first < b.first;
    };
    std::sort(vertexRangesToRemove.begin(), vertexRangesToRemove.end(), rangeComparer);
    std::sort(indexRangesToRemove.begin(), indexRangesToRemove.end(), rangeComparer);

    for (auto& pair : EntityToVertexRange)
    {
        int vertexAdjustment = 0;
        int indexAdjustment = 0;

        auto& entity = pair.first;
        auto& indexRange = EntityToIndexRange[entity];
        auto& vertexRange = EntityToVertexRange[entity];

        for (const auto& removedRange : vertexRangesToRemove)
        {
            if (removedRange.first < vertexRange.first)
            {
                vertexAdjustment += removedRange.second - removedRange.first;
            }
        }
        for (const auto& removedRange : indexRangesToRemove)
        {
            if (removedRange.first < indexRange.first)
            {
                indexAdjustment += removedRange.second - removedRange.first;
            }
        }

        if (vertexAdjustment > 0)
        {
            for (int i = indexRange.first; i < indexRange.second; i++)
            {
                m_IndexBuffer->Buffer[i] -= vertexAdjustment;
            }
            vertexRange.first -= vertexAdjustment;
            vertexRange.second -= vertexAdjustment;
            indexRange.first -= indexAdjustment;
            indexRange.second -= indexAdjustment;
        }
    }

    Utils::EraseRanges(vertexRangesToRemove, m_VertexBuffer->Buffer);
    Utils::EraseRanges(indexRangesToRemove, m_IndexBuffer->Buffer);

    UpdateCoreInterval();
}

//------------------------------------------------------------------------
//  Recomputes positions/normals if the transform changed
//------------------------------------------------------------------------
void MeshHandler::UpdateMeshTransform(Entity entity, Transform& transform)
{
    BufferRange range = m_RenderConstants->EntityToVertexRange[entity];
    auto begin = m_VertexBuffer->Buffer.begin() + range.first;
    auto end = m_VertexBuffer->Buffer.begin() + range.second;
    Transform T = transform.GetWorldTransform();
    Concurrent::ForEach(begin, end, [&](Vertex& v) {
        v.Position = T.TransformVec3(v.LocalPosition);
        v.Normal = T.TransformNormal(v.LocalNormal);
    });
    transform.IsDirty = false;
}

//------------------------------------------------------------------------
//  Updates the fragment shader ID in the existing vertex buffer range
//------------------------------------------------------------------------
void MeshHandler::UpdateMeshFragShader(Entity entity, size_t shaderID)
{
    BufferRange range = m_RenderConstants->EntityToVertexRange[entity];
    auto begin = m_VertexBuffer->Buffer.begin() + range.first;
    auto end = m_VertexBuffer->Buffer.begin() + range.second;
    std::for_each(begin, end, [&](Vertex& v) { v.FragShaderID = shaderID; });
}

void MeshHandler::UpdateMeshVertShader(Entity entity, size_t shaderID)
{
    BufferRange range = m_RenderConstants->EntityToVertexRange[entity];
    auto begin = m_VertexBuffer->Buffer.begin() + range.first;
    auto end = m_VertexBuffer->Buffer.begin() + range.second;
    std::for_each(begin, end, [&](Vertex& v) { v.VertexShaderID = shaderID; });
}

//------------------------------------------------------------------------
//  Add a mesh from an asset
//------------------------------------------------------------------------
void MeshHandler::AddMesh(
        Entity entity, Mesh mesh, Transform& transform, size_t FragShaderID, size_t VertShaderID)
{

    // Retrieve the mesh data from the AssetServer
    MeshInstance instance = AssetServer::GetInstance().GetModel(mesh.Model);

    // Transform vertices into world space
    instance.transform(transform);

    // Ensure each vertex has the correct shader ID
    for (auto& vertex : instance.vertices)
    {
        vertex.FragShaderID = FragShaderID;
        vertex.VertexShaderID = VertShaderID;
    }

    // Call the common logic
    AddMeshCommon(entity, instance.vertices, instance.indices, FragShaderID, VertShaderID);

    UpdateCoreInterval();
}
