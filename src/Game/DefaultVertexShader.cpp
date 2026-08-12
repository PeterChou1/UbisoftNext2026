#include "DefaultVertexShader.h"

#include "stdafx.h"

void DefaultVertexShader::Shade(Vertex& v, Camera& cam, DirectionalLight& light)
{
    v.PositionCamera = cam.WorldToCamera(v.Position);
    v.Projection = cam.Proj * Vec4(v.PositionCamera);
    if (ShadowMapping)
    {
        Vec3 LightSpace = light.WorldToLightSpace(v.Position);
        v.ShadowProjection = light.Proj * Vec4(LightSpace);
    }
}
