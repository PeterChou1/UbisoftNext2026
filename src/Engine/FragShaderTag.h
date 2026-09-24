//---------------------------------------------------------------------------------
// Shader.h
//---------------------------------------------------------------------------------
//
// Provides a simple wrapper for users to apply different Meshes
//

#pragma once

#include "Assets.h"

struct FragShaderTag
{
    // Determines what type of Fragment Shader to use
    FragShaderTypeID FragAssetId;
    // Internal ID to keep track of specific
    size_t FragShaderID = 0;

    bool Initialized = false;

    FragShaderTag()
        : FragAssetId(DefaultFragShaderID)
    {
    }

    FragShaderTag(FragShaderTypeID ShaderId)
        : FragAssetId(ShaderId)
    {
    }
};
