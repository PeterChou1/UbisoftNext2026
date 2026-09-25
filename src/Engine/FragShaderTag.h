//---------------------------------------------------------------------------------
// FragShaderTag.h
//---------------------------------------------------------------------------------
//
// Component choosing the fragment shader of an entity's mesh / shape
//

#pragma once

#include "Assets.h"

struct FragShaderTag
{
    // Determines what type of Fragment Shader to use
    FragShaderTypeID FragAssetId;
    // The entity's shader instance in the AssetServer (0: the default shader)
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
