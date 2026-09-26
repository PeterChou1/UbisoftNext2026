//---------------------------------------------------------------------------------
// ClippedTriangleBuffer.h
//---------------------------------------------------------------------------------
//
// ClippedTriangleBuffer stores the clipped triangles when the Clipper is done
// clipping them for use in the next stage of the pipeline
//
#pragma once
#include "Resource.h"
#include "Triangle.h"

#include <thread>
#include <vector>

class ClippedTriangleBuffer : public Resource
{
  public:
    ClippedTriangleBuffer()
        : CameraClipBuffer(std::thread::hardware_concurrency())
        , LightClipBuffer(std::thread::hardware_concurrency())
    {
    }

    void ResetResource() override
    {
        for (auto& triangles : CameraClipBuffer)
            triangles.clear();
        for (auto& triangles : LightClipBuffer)
            triangles.clear();
    }

    // One bin per clipping thread, from the camera and from the light
    std::vector<std::vector<Triangle>> CameraClipBuffer;
    std::vector<std::vector<Triangle>> LightClipBuffer;
};
