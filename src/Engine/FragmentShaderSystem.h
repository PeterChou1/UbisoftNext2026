//---------------------------------------------------------------------------------
// FragmentShaderSystem.h
//---------------------------------------------------------------------------------
//
// Multi-threaded fragment stage: runs each visible pixel's fragment shader
// (8 pixels at a time) and draws the resulting colour buffer
//
#pragma once
#include "Camera.h"
#include "ClippedTriangleBuffer.h"
#include "ColorBuffer.h"
#include "GameOptions.h"
#include "Lighting.h"
#include "PixelBuffer.h"

class FragmentShaderSystem
{
  public:
    FragmentShaderSystem();

    void Shade();

  private:
    std::shared_ptr<ClippedTriangleBuffer> m_ClippedTriangle;
    std::shared_ptr<PixelBuffer> m_PixelBuffer;
    std::shared_ptr<ColorBuffer> m_ColorBuffer;
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<DepthBuffer> m_DepthBuffer;
};
