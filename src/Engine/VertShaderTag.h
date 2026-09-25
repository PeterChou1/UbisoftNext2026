//---------------------------------------------------------------------------------
// VertShaderTag.h
//---------------------------------------------------------------------------------
//
// Component choosing the vertex shader of an entity's mesh / shape
//
#pragma once
#include "Assets.h"

struct VertShaderTag
{
    // Determines what type of Vertex Shader to use
    VertShaderTypeID VertAssetId;
    // The entity's shader instance in the AssetServer (0: the default shader)
    size_t VertShaderID = 0;

    bool Initialized = false;

    VertShaderTag()
        : VertAssetId(DefaultVertShaderID)
    {
    }

    VertShaderTag(VertShaderTypeID ShaderId)
        : VertAssetId(ShaderId)
    {
    }
};
