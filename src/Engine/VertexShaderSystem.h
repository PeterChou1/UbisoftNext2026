//---------------------------------------------------------------------------------
// VertexShaderSystem.h
//---------------------------------------------------------------------------------
//
// Multithreaded vertex stage: each vertex of the vertex buffer is projected
// to clip space by its vertex shader
//
#pragma once
#include "Camera.h"
#include "Lighting.h"
#include "VertexBuffer.h"

#include <memory>

class VertexShaderSystem
{
  public:
    VertexShaderSystem();

    void Shade();

  private:
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Lighting;
};
