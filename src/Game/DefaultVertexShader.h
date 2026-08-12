#pragma once
#include "VertexShader.h"

class DefaultVertexShader : public VertexShader
{
  public:
    void Shade(Vertex& v, Camera& cam, DirectionalLight& light) override;

    ~DefaultVertexShader() override = default;
};
