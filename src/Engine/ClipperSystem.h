//---------------------------------------------------------------------------------
// ClipperSystem.h
//---------------------------------------------------------------------------------
// Multi-threaded Clipper that runs on all cores of the machine
// Clips the triangle and outputs the clipped Triangle to raster space
// See:
// https://chaosinmotion.com/2016/05/22/3d-clipping-in-homogeneous-coordinates/
//
#pragma once

#include "Camera.h"
#include "ClippedTriangleBuffer.h"
#include "DepthBuffer.h"
#include "GameOptions.h"
#include "IndexBuffer.h"
#include "Lighting.h"
#include "RenderConstants.h"
#include "VertexBuffer.h"

#include <memory>

class ClipperSystem
{
  public:
    ClipperSystem();

    void Clip();

  private:
    std::shared_ptr<GameOptions> m_GameOptions;
    std::shared_ptr<DepthBuffer> m_DepthBuffer;
    std::shared_ptr<RenderConstants> m_RenderConstants;
    std::shared_ptr<IndexBuffer> m_IndexBuffer;
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    std::shared_ptr<ClippedTriangleBuffer> m_ClippedTriangleBuffer;
    std::shared_ptr<Lighting> m_Lighting;
};
