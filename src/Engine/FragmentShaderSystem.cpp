#include "FragmentShaderSystem.h"

#include "AssetServer.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

FragmentShaderSystem::FragmentShaderSystem()
{
    m_DepthBuffer = ECS.GetResource<DepthBuffer>();
    m_PixelBuffer = ECS.GetResource<PixelBuffer>();
    m_ClippedTriangle = ECS.GetResource<ClippedTriangleBuffer>();
    m_ColorBuffer = ECS.GetResource<ColorBuffer>();
    m_Cam = ECS.GetResource<Camera>();
    m_Light = ECS.GetResource<Lighting>();
    m_Options = ECS.GetResource<GameOptions>();
}

void FragmentShaderSystem::Shade()
{
    if (m_Options->LineRendering)
        return;
    AssetServer& loader = AssetServer::GetInstance();
    std::vector<std::vector<Triangle>>& clippedTriangles = m_ClippedTriangle->CameraClipBuffer;
    DirectionalLight& light = m_Light->GetDirectionalLight();
    Concurrent::ForEach(m_PixelBuffer->begin(), m_PixelBuffer->end(), [&](SIMDPixel& pixel) {
        Triangle& triangle = clippedTriangles[pixel.BinId][pixel.BinIndex];
        pixel.Interpolate(triangle);
        Material& texture = loader.GetMaterial(triangle.GetTextureID());
        loader.GetFragShader(triangle.GetShaderID())
                ->Shade(pixel, *m_DepthBuffer, texture, *m_Cam, light);
        SIMDVec3 color = pixel.Color * 255.0;
        color.X = color.X.floor();
        color.Y = color.Y.floor();
        color.Z = color.Z.floor();
        for (int lane = 0; lane < SIMDPixel::PIXEL_WIDTH * SIMDPixel::PIXEL_HEIGHT; ++lane)
        {
            if (!pixel.Mask.GetBit(lane))
                continue;
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[lane]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[lane]),
                                    static_cast<unsigned char>(color.X[lane]),
                                    static_cast<unsigned char>(color.Y[lane]),
                                    static_cast<unsigned char>(color.Z[lane]));
        }
    });

    for (int y = 0; y < APP_VIRTUAL_HEIGHT; y++)
    {
        for (int x = 0; x < APP_VIRTUAL_WIDTH; x++)
        {
            unsigned char r, g, b;
            m_ColorBuffer->GetColor(x, y, r, g, b);
            App::DrawLine(static_cast<float>(x),
                          static_cast<float>(y),
                          static_cast<float>(x) + 1.0f,
                          static_cast<float>(y) + 1.0f,
                          static_cast<float>(r / 255.0),
                          static_cast<float>(g / 255.0),
                          static_cast<float>(b / 255.0));
        }
    }
}
