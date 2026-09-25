#pragma once
#include "Camera.h"
#include "Lights.h"
#include "Vertex.h"

class VertexShader
{
  public:
    // Time the shader has run (seconds), advanced by the ShaderHandler
    float DeltaTime = 0;
    bool ShadowMapping = false;

    virtual void Shade(Vertex& v, Camera& cam, DirectionalLight& light) = 0;

    virtual ~VertexShader() = default;

  protected:
    // Project a world position into the vertex's camera (and light) space
    void Project(Vertex& v, Vec3 world, Camera& cam, DirectionalLight& light) const
    {
        v.PositionCamera = cam.WorldToCamera(world);
        v.Projection = cam.Proj * Vec4(v.PositionCamera);
        if (ShadowMapping)
            v.ShadowProjection = light.Proj * Vec4(light.WorldToLightSpace(world));
    }
};
