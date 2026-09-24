//---------------------------------------------------------------------------------
// VertexShader.h
//---------------------------------------------------------------------------------
//
// Multithreaded Vertex Shader outputs vertexs in the vertex
// buffer to clip space
//
#pragma once
#include "Camera.h"
#include "GameOptions.h"
#include "Lighting.h"
#include "VertexBuffer.h"

#include <memory>

class VertexShaderSystem
{
  public:
    VertexShaderSystem();

    void Shade();

  private:
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    std::shared_ptr<Camera> m_cam;
    std::shared_ptr<Lighting> m_Lighting;
};
