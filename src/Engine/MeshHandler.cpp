#include "MeshHandler.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "Emitter.h"
#include "FragShaderTag.h"
#include "Mesh.h"
#include "Utils.h"
#include "VertShaderTag.h"
#include "World/SceneComponents.h"
#include "World/ShapeGeometry.h"
#include "stdafx.h"

#include <algorithm>

extern ECSManager ECS;

namespace
{
    // The shader instance IDs of an entity's shader tags (0: the default shader)
    size_t FragShaderOf(Entity e)
    {
        return ECS.HasComponent<FragShaderTag>(e) ? ECS.GetComponent<FragShaderTag>(e).FragShaderID
                                                  : 0;
    }

    size_t VertShaderOf(Entity e)
    {
        return ECS.HasComponent<VertShaderTag>(e) ? ECS.GetComponent<VertShaderTag>(e).VertShaderID
                                                  : 0;
    }
} // namespace

MeshHandler::MeshHandler()
{
    m_RenderConstants = ECS.GetResource<RenderConstants>();
    m_VertexBuffer = ECS.GetResource<VertexBuffer>();
    m_IndexBuffer = ECS.GetResource<IndexBuffer>();
    m_Cam = ECS.GetResource<Camera>();
}

void MeshHandler::Update()
{
    // -- 1. Models --------------------------------------------------------------
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

        const size_t fragShaderID = FragShaderOf(e);
        const size_t vertShaderID = VertShaderOf(e);
        if (!mesh.Loaded)
        {
            // New, or the model changed: replace any previous geometry
            if (m_RenderConstants->EntityToVertexRange.count(e) != 0)
                DeleteMeshes({e});
            AddModel(e, mesh.Model, transform, fragShaderID, vertShaderID);
            mesh.Loaded = true;
        }
        else if (transform.IsDirty)
            UpdateMeshTransform(e, transform);
        UpdateMeshShaders(e, fragShaderID, vertShaderID);
    }

    // -- 2. 2D shapes (procedural meshes) ---------------------------------------
    for (const auto e : ECS.Visit<Transform, Shape2D>())
    {
        auto& shape = ECS.GetComponent<Shape2D>(e);
        auto& transform = ECS.GetComponent<Transform>(e);

        const size_t fragShaderID = FragShaderOf(e);
        const size_t vertShaderID = VertShaderOf(e);
        if (!shape.Built)
        {
            // (Re)build: the shape is new or was edited (size, colour ...)
            if (m_RenderConstants->EntityToVertexRange.count(e) != 0)
                DeleteMeshes({e});
            ShapeGeometry::MeshData data = ShapeGeometry::BuildMesh(shape);
            AddMesh(e, std::move(data.Vertices), data.Indices, fragShaderID, vertShaderID);
            shape.Built = true;
            UpdateMeshTransform(e, transform);
        }
        else if (transform.IsDirty)
            UpdateMeshTransform(e, transform);
        UpdateMeshShaders(e, fragShaderID, vertShaderID);
    }

    // -- 3. Particles: a quad facing the camera ---------------------------------
    for (const auto e : ECS.Visit<Transform, Particle, FragShaderTag>())
    {
        auto& particle = ECS.GetComponent<Particle>(e);
        auto& transform = ECS.GetComponent<Transform>(e);

        if (!particle.loaded)
        {
            const Vec3 right = m_Cam->CamTransform.GetRight();
            const Vec3 up = m_Cam->CamTransform.GetUp();
            const float halfSize = 0.1f;
            std::vector<Vertex> vertices(4);
            vertices[0].LocalPosition = right * -halfSize - up * halfSize;
            vertices[1].LocalPosition = right * halfSize - up * halfSize;
            vertices[2].LocalPosition = right * halfSize + up * halfSize;
            vertices[3].LocalPosition = right * -halfSize + up * halfSize;
            Vec3 lineA = vertices[0].LocalPosition - vertices[1].LocalPosition;
            Vec3 lineB = vertices[0].LocalPosition - vertices[2].LocalPosition;
            Vec3 normal = lineA.Cross(lineB);
            normal.Normalize();
            for (auto& v : vertices)
            {
                v.LocalNormal = normal;
                v.Color = particle.Color;
            }
            AddMesh(e,
                    std::move(vertices),
                    {0, 1, 2, 2, 3, 0},
                    ECS.GetComponent<FragShaderTag>(e).FragShaderID,
                    0);
            particle.loaded = true;
        }

        if (transform.IsDirty)
            UpdateMeshTransform(e, transform);
    }

    // -- 4. Entities that lost their Particle, Mesh or Shape2D ------------------
    DeleteDestroyed();
}

void MeshHandler::DeleteDestroyed()
{
    std::vector<Entity> destroyed;
    for (const auto e : ECS.VisitDeleted<Particle>())
        destroyed.push_back(e);
    for (const auto e : ECS.VisitDeleted<Mesh>())
        destroyed.push_back(e);
    for (const auto e : ECS.VisitDeleted<Shape2D>())
        destroyed.push_back(e);

    // DeleteMeshes ignores entities whose geometry is already gone
    if (!destroyed.empty())
        DeleteMeshes(destroyed);
}

void MeshHandler::UpdateCoreInterval()
{
    m_RenderConstants->TriangleCount = static_cast<uint32_t>(m_IndexBuffer->Buffer.size() / 3);
    const unsigned int coreCount = m_RenderConstants->CoreCount;
    m_RenderConstants->CoreInterval =
            (m_RenderConstants->TriangleCount + coreCount - 1) / coreCount;
}

void MeshHandler::AddMesh(Entity entity,
                          std::vector<Vertex> vertices,
                          const std::vector<std::uint32_t>& indices,
                          size_t fragShaderID,
                          size_t vertShaderID)
{
    for (auto& vertex : vertices)
    {
        vertex.FragShaderID = fragShaderID;
        vertex.VertexShaderID = vertShaderID;
    }

    // Record the ranges in the vertex/index buffers
    const int offsetVertex = static_cast<int>(m_VertexBuffer->Buffer.size());
    m_RenderConstants->EntityToVertexRange[entity] = {
            offsetVertex, offsetVertex + static_cast<int>(vertices.size())};
    const int offsetIndex = static_cast<int>(m_IndexBuffer->Buffer.size());
    m_RenderConstants->EntityToIndexRange[entity] = {
            offsetIndex, offsetIndex + static_cast<int>(indices.size())};
    m_RenderConstants->EntityToFragShaderID[entity] = fragShaderID;
    m_RenderConstants->EntityToVertShaderID[entity] = vertShaderID;

    m_VertexBuffer->Buffer.insert(m_VertexBuffer->Buffer.end(), vertices.begin(), vertices.end());
    for (auto id : indices)
        m_IndexBuffer->Buffer.push_back(offsetVertex + id);

    UpdateCoreInterval();
}

void MeshHandler::AddModel(Entity entity,
                           const std::string& model,
                           Transform& transform,
                           size_t fragShaderID,
                           size_t vertShaderID)
{
    MeshInstance instance = AssetServer::GetInstance().GetModel(model);
    // Into world space (through the parents of a child)
    instance.transform(transform.GetWorldTransform());
    AddMesh(entity, std::move(instance.vertices), instance.indices, fragShaderID, vertShaderID);
}

void MeshHandler::DeleteMeshes(const std::vector<Entity>& entities)
{
    std::vector<BufferRange> vertexRangesToRemove;
    std::vector<BufferRange> indexRangesToRemove;
    auto& entityToVertexRange = m_RenderConstants->EntityToVertexRange;
    auto& entityToIndexRange = m_RenderConstants->EntityToIndexRange;

    for (const auto& entity : entities)
    {
        if (entityToVertexRange.count(entity))
        {
            vertexRangesToRemove.push_back(entityToVertexRange[entity]);
            entityToVertexRange.erase(entity);
        }
        if (entityToIndexRange.count(entity))
        {
            indexRangesToRemove.push_back(entityToIndexRange[entity]);
            entityToIndexRange.erase(entity);
        }
        m_RenderConstants->EntityToFragShaderID.erase(entity);
        m_RenderConstants->EntityToVertShaderID.erase(entity);
    }

    auto byStart = [](const BufferRange& a, const BufferRange& b) { return a.first < b.first; };
    std::sort(vertexRangesToRemove.begin(), vertexRangesToRemove.end(), byStart);
    std::sort(indexRangesToRemove.begin(), indexRangesToRemove.end(), byStart);

    // Shift the remaining meshes over the removed ranges
    for (auto& [entity, vertexRange] : entityToVertexRange)
    {
        auto& indexRange = entityToIndexRange[entity];
        int vertexAdjustment = 0;
        int indexAdjustment = 0;
        for (const auto& removedRange : vertexRangesToRemove)
        {
            if (removedRange.first < vertexRange.first)
                vertexAdjustment += removedRange.second - removedRange.first;
        }
        for (const auto& removedRange : indexRangesToRemove)
        {
            if (removedRange.first < indexRange.first)
                indexAdjustment += removedRange.second - removedRange.first;
        }

        if (vertexAdjustment > 0)
        {
            for (int i = indexRange.first; i < indexRange.second; i++)
                m_IndexBuffer->Buffer[i] -= vertexAdjustment;
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

void MeshHandler::UpdateMeshTransform(Entity entity, Transform& transform)
{
    BufferRange range = m_RenderConstants->EntityToVertexRange[entity];
    auto begin = m_VertexBuffer->Buffer.begin() + range.first;
    auto end = m_VertexBuffer->Buffer.begin() + range.second;
    Transform world = transform.GetWorldTransform();
    Concurrent::ForEach(begin, end, [&](Vertex& v) {
        v.Position = world.TransformVec3(v.LocalPosition);
        v.Normal = world.TransformNormal(v.LocalNormal);
    });
    transform.IsDirty = false;
}

void MeshHandler::UpdateMeshShaders(Entity entity, size_t fragShaderID, size_t vertShaderID)
{
    auto forEachVertex = [&](auto set) {
        BufferRange range = m_RenderConstants->EntityToVertexRange[entity];
        std::for_each(m_VertexBuffer->Buffer.begin() + range.first,
                      m_VertexBuffer->Buffer.begin() + range.second,
                      set);
    };
    auto& fragShaderIDs = m_RenderConstants->EntityToFragShaderID;
    if (fragShaderIDs.count(entity) != 0 && fragShaderIDs[entity] != fragShaderID)
    {
        fragShaderIDs[entity] = fragShaderID;
        forEachVertex([&](Vertex& v) { v.FragShaderID = fragShaderID; });
    }
    auto& vertShaderIDs = m_RenderConstants->EntityToVertShaderID;
    if (vertShaderIDs.count(entity) != 0 && vertShaderIDs[entity] != vertShaderID)
    {
        vertShaderIDs[entity] = vertShaderID;
        forEachVertex([&](Vertex& v) { v.VertexShaderID = vertShaderID; });
    }
}
