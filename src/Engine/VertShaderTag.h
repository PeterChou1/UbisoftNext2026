#pragma once
#include "Assets.h"

struct VertShaderTag
{
    VertShaderTypeID VertAssetId;

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
