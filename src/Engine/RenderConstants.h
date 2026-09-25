//---------------------------------------------------------------------------------
// RenderConstants.h
//---------------------------------------------------------------------------------
//
// Bookkeeping of the rendering process: where each entity's geometry is in
// the vertex / index buffers and how the triangles are split between cores
//
#pragma once
#include "Assets.h"
#include "Entity.h"
#include "Resource.h"

#include <numeric>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

using BufferRange = std::pair<int, int>;

class RenderConstants : public Resource
{
  public:
    RenderConstants()
    {
        CoreCount = std::thread::hardware_concurrency();
        CoreIds.resize(CoreCount);
        std::iota(CoreIds.begin(), CoreIds.end(), 0);
    }

    void ResetResource() override
    {
        EntityToIndexRange.clear();
        EntityToVertexRange.clear();
        CoreInterval = 0;
        TriangleCount = 0;
    }

    // The shader instance IDs of each mesh entity's vertices (MeshHandler)
    // and the shader types of each entity's tags (ShaderHandler), used to
    // detect changes
    std::unordered_map<Entity, size_t> EntityToVertShaderID;
    std::unordered_map<Entity, size_t> EntityToFragShaderID;

    std::unordered_map<Entity, VertShaderTypeID> EntityToVertShaderType;
    std::unordered_map<Entity, FragShaderTypeID> EntityToFragShaderType;
    // Maps which entity has which index/vertex ranges in the index and
    // vertex buffer
    std::unordered_map<Entity, BufferRange> EntityToVertexRange;
    std::unordered_map<Entity, BufferRange> EntityToIndexRange;
    // Gives each core on the system an ID
    std::vector<std::uint32_t> CoreIds;
    // Used by clipper during multithreading clipping
    int CoreInterval{};
    unsigned int CoreCount{};
    // total triangle count of Triangle in the System
    std::uint32_t TriangleCount{};
};