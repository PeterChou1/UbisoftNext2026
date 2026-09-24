//---------------------------------------------------------------------------------
// RenderConstants.h
//---------------------------------------------------------------------------------
//
// Render Constants is responsible to keeping track with
// constants related to the rendering process
//
#pragma once
#include "Assets.h"
#include "Entity.h"
#include "Resource.h"
#include "Vertex.h"

#include <numeric>
#include <thread>
#include <unordered_map>

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
        EntityToIndexRange.clear();
        CoreInterval = 0;
        TriangleCount = 0;
    }

    std::unordered_map<Entity, std::vector<BufferRange>> EntityToAnimation;

    // Maps which mesh entity has which vertex shaders attach to it
    // used to detect changes in vertex/fragment shader
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