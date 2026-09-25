#pragma once
#include "VertexShader.h"

// Projects each vertex where it is, without animation
class DefaultVertexShader : public VertexShader
{
  public:
    void Shade(Vertex& v, Camera& cam, DirectionalLight& light) override
    {
        Project(v, v.Position, cam, light);
    }
};
