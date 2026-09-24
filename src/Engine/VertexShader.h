#pragma once
#include "Camera.h"
#include "Lights.h"
#include "Vertex.h"

class VertexShader
{
  public:
    float DeltaTime = 0;
    bool ShadowMapping = false;

    virtual void Shade(Vertex& v, Camera& cam, DirectionalLight& light) = 0;

    virtual ~VertexShader() = default;
};
