#include "FragmentShaderSystem.h"

#include "AssetServer.h"
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
    std::vector<std::vector<Triangle>> ClippedTriangle = m_ClippedTriangle->CameraClipBuffer;
    DirectionalLight& Lighting = m_Light->GetDirectionalLight();
    Concurrent::ForEach(m_PixelBuffer->begin(), m_PixelBuffer->end(), [&](SIMDPixel& pixel) {
        Triangle& triangle = ClippedTriangle[pixel.BinId][pixel.BinIndex];
        pixel.Interpolate(triangle);
        std::shared_ptr<FragmentShader> shader = loader.GetFragShader(triangle.GetShaderID());
        Material& texture = loader.GetMaterial(triangle.GetTextureID());
        shader->Shade(pixel, *m_DepthBuffer, texture, *m_Cam, Lighting);
        SIMDVec3 color = pixel.Color * 255.0;
        if (SIMD::Any(color.Y > SIMD::ZERO))
        {
            int x = 0;
        }
        color.X = color.X.floor();
        color.Y = color.Y.floor();
        color.Z = color.Z.floor();

        if (pixel.Mask.GetBit(0))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[0]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[0]),
                                    static_cast<unsigned char>(color.X[0]),
                                    static_cast<unsigned char>(color.Y[0]),
                                    static_cast<unsigned char>(color.Z[0]));
        }

        if (pixel.Mask.GetBit(1))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[1]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[1]),
                                    static_cast<unsigned char>(color.X[1]),
                                    static_cast<unsigned char>(color.Y[1]),
                                    static_cast<unsigned char>(color.Z[1]));
        }

        if (pixel.Mask.GetBit(2))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[2]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[2]),
                                    static_cast<unsigned char>(color.X[2]),
                                    static_cast<unsigned char>(color.Y[2]),
                                    static_cast<unsigned char>(color.Z[2]));
        }

        if (pixel.Mask.GetBit(3))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[3]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[3]),
                                    static_cast<unsigned char>(color.X[3]),
                                    static_cast<unsigned char>(color.Y[3]),
                                    static_cast<unsigned char>(color.Z[3]));
        }

        if (pixel.Mask.GetBit(4))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[4]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[4]),
                                    static_cast<unsigned char>(color.X[4]),
                                    static_cast<unsigned char>(color.Y[4]),
                                    static_cast<unsigned char>(color.Z[4]));
        }

        if (pixel.Mask.GetBit(5))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[5]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[5]),
                                    static_cast<unsigned char>(color.X[5]),
                                    static_cast<unsigned char>(color.Y[5]),
                                    static_cast<unsigned char>(color.Z[5]));
        }

        if (pixel.Mask.GetBit(6))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[6]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[6]),
                                    static_cast<unsigned char>(color.X[6]),
                                    static_cast<unsigned char>(color.Y[6]),
                                    static_cast<unsigned char>(color.Z[6]));
        }

        if (pixel.Mask.GetBit(7))
        {
            m_ColorBuffer->SetColor(static_cast<int>(pixel.ScreenSpacePosition.X[7]),
                                    static_cast<int>(pixel.ScreenSpacePosition.Y[7]),
                                    static_cast<unsigned char>(color.X[7]),
                                    static_cast<unsigned char>(color.Y[7]),
                                    static_cast<unsigned char>(color.Z[7]));
        }
    });
    int HStart = (APP_VIRTUAL_HEIGHT - m_Options->VirtualHeight) / 2;
    int WStart = (APP_VIRTUAL_WIDTH - m_Options->VirtualWidth) / 2;

    for (int y = 0; y < m_Options->VirtualHeight; y++)
    {
        for (int x = 0; x < m_Options->VirtualWidth; x++)
        {
            unsigned char r, g, b;

            m_ColorBuffer->GetColor(x, y, r, g, b);
            App::DrawLine(static_cast<float>(x + WStart),
                          static_cast<float>(y + HStart),
                          static_cast<float>(x + WStart) + 1.0f,
                          static_cast<float>(y + HStart) + 1.0f,
                          static_cast<float>(r / 255.0),
                          static_cast<float>(g / 255.0),
                          static_cast<float>(b / 255.0));
        }
    }
}