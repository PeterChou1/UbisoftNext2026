//---------------------------------------------------------------------------------
// DepthBuffer.h
//---------------------------------------------------------------------------------
//
// The DepthBuffer is used to store all depth information during rasterization:
// the camera's depth buffer and the light's (the shadow map), in blocks of
// SIMDPixel::PIXEL_WIDTH x PIXEL_HEIGHT texels. Larger values are closer
//
#pragma once

#include "Resource.h"
#include "SIMDPixel.h"
#include "Utils.h"

#include <cassert>
#include <vector>

class DepthBuffer : public Resource
{
  public:
    DepthBuffer(int width, int height, int widthS, int heightS)
        : m_CamHeight(height / SIMDPixel::PIXEL_HEIGHT)
        , m_CamWidth(width / SIMDPixel::PIXEL_WIDTH)
        , m_ShadowHeight(heightS / SIMDPixel::PIXEL_HEIGHT)
        , m_ShadowWidth(widthS / SIMDPixel::PIXEL_WIDTH)
    {
        m_DepthCamBuffer.resize(m_CamHeight * m_CamWidth);
        m_DepthShadowBuffer.resize(m_ShadowHeight * m_ShadowWidth);
    }

    // Size of the shadow map in texels
    int ShadowTexelsX() const { return m_ShadowWidth * SIMDPixel::PIXEL_WIDTH; }
    int ShadowTexelsY() const { return m_ShadowHeight * SIMDPixel::PIXEL_HEIGHT; }

    // From the light's NDC to shadow map texels
    void ToShadowSpace(Vec4& point) const
    {
        float width = static_cast<float>(ShadowTexelsX());
        float height = static_cast<float>(ShadowTexelsY());
        point.X = static_cast<float>(static_cast<int>((point.X + 1) * 0.5 * width));
        point.Y = static_cast<float>(static_cast<int>((point.Y + 1) * 0.5 * height));
        point.X = Utils::Clamp(point.X, 0, width);
        point.Y = Utils::Clamp(point.Y, 0, height);
    }

    // The pixels of block (x, y) inside the mask that are closer than the buffer
    SIMDFloat DepthTest(int x, int y, SIMDFloat& depth, SIMDFloat& mask, bool shadow)
    {
        return mask & (depth > Block(x, y, shadow));
    }

    void UpdateBuffer(int x, int y, SIMDFloat& mask, SIMDFloat& depth, bool shadow)
    {
        SIMDFloat& curDepth = Block(x, y, shadow);
        curDepth = SIMD::Select(mask, curDepth, depth);
    }

    // Depth of shadow map texel (x, y), 1 (closest) outside the map
    float ShadowDepth(int x, int y) const
    {
        const int xAlign = x / SIMDPixel::PIXEL_WIDTH;
        const int yAlign = y / SIMDPixel::PIXEL_HEIGHT;
        if (x < 0 || y < 0 || xAlign >= m_ShadowWidth || yAlign >= m_ShadowHeight)
            return 1.0f;
        const int lane =
                (y % SIMDPixel::PIXEL_HEIGHT) * SIMDPixel::PIXEL_WIDTH + x % SIMDPixel::PIXEL_WIDTH;
        return m_DepthShadowBuffer[yAlign * m_ShadowWidth + xAlign].V[lane];
    }

    // Camera depths of the block at an index
    SIMDFloat CameraDepth(int index) const
    {
        assert(0 <= index && index < static_cast<int>(m_DepthCamBuffer.size()));
        return m_DepthCamBuffer[index];
    }

    void ResetResource() override
    {
        for (auto& depth : m_DepthCamBuffer)
            depth = SIMDFloat(0.0f);
        for (auto& depth : m_DepthShadowBuffer)
            depth = SIMDFloat(0.0f);
    }

  private:
    SIMDFloat& Block(int x, int y, bool shadow)
    {
        return shadow ? m_DepthShadowBuffer[y * m_ShadowWidth + x]
                      : m_DepthCamBuffer[y * m_CamWidth + x];
    }

    int m_CamHeight;
    int m_CamWidth;
    int m_ShadowHeight;
    int m_ShadowWidth;
    std::vector<SIMDFloat> m_DepthShadowBuffer;
    std::vector<SIMDFloat> m_DepthCamBuffer;
};
