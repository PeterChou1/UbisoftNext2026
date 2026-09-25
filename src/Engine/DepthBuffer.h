//---------------------------------------------------------------------------------
// DepthBuffer.h
//---------------------------------------------------------------------------------
//
// The DepthBuffer is used to store all depth information during rasterization
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
        , m_ShadowWidth(widthS / SIMDPixel::PIXEL_WIDTH)
        , m_ShadowHeight(heightS / SIMDPixel::PIXEL_HEIGHT)
    {
        m_DepthCamBuffer.resize(m_CamHeight * m_CamWidth);
        m_DepthShadowBuffer.resize(m_ShadowHeight * m_ShadowWidth);
    }

    void SetDepthSize(int width, int height)
    {
        m_CamHeight = height / SIMDPixel::PIXEL_HEIGHT;
        m_CamWidth = width / SIMDPixel::PIXEL_WIDTH;
        m_DepthCamBuffer.resize(m_ShadowHeight * m_ShadowWidth);
    }

    void SetShadowDepthSize(int width, int height)
    {
        m_ShadowHeight = height / SIMDPixel::PIXEL_HEIGHT;
        m_ShadowWidth = width / SIMDPixel::PIXEL_WIDTH;
        m_DepthShadowBuffer.resize(m_ShadowHeight * m_ShadowWidth);
    }

    // Size of the shadow map in texels
    int ShadowTexelsX() const { return m_ShadowWidth * SIMDPixel::PIXEL_WIDTH; }
    int ShadowTexelsY() const { return m_ShadowHeight * SIMDPixel::PIXEL_HEIGHT; }

    void ToShadowSpace(Vec4& point) const
    {
        float width = static_cast<float>(SIMDPixel::PIXEL_WIDTH * m_ShadowWidth);
        float height = static_cast<float>(SIMDPixel::PIXEL_HEIGHT * m_ShadowHeight);
        point.X = static_cast<float>(static_cast<int>((point.X + 1) * 0.5 * width));
        point.Y = static_cast<float>(static_cast<int>((point.Y + 1) * 0.5 * height));
        point.X = Utils::Clamp(point.X, 0, width);
        point.Y = Utils::Clamp(point.Y, 0, height);
    }

    SIMDFloat DepthTest(int x, int y, SIMDFloat& depth, SIMDFloat& mask, bool shadow)
    {
        int width = shadow ? m_ShadowWidth : m_CamWidth;
        SIMDFloat& curDepth =
                shadow ? m_DepthShadowBuffer[y * width + x] : m_DepthCamBuffer[y * width + x];
        const SIMDFloat visible = depth > curDepth;
        const SIMDFloat updateMask = mask & visible;
        return updateMask;
    }

    SIMDFloat GetDepthSIMD(SIMDVec2& location, bool shadow) const
    {
        SIMDFloat Depth{};
        SIMDFloat x = location.X;
        SIMDFloat y = location.Y;
        int limit = SIMDPixel::PIXEL_WIDTH * SIMDPixel::PIXEL_HEIGHT;
        for (int i = 0; i < limit; i++)
        {
            Depth.V[i] = GetBufferSingle(static_cast<int>(x[i]), static_cast<int>(y[i]), shadow);
        }
        return Depth;
    }

    void UpdateBuffer(int x, int y, SIMDFloat& mask, SIMDFloat& depth, bool shadow)
    {
        int width = shadow ? m_ShadowWidth : m_CamWidth;
        SIMDFloat& curDepth =
                shadow ? m_DepthShadowBuffer[y * width + x] : m_DepthCamBuffer[y * width + x];
        curDepth = SIMD::Select(mask, curDepth, depth);
    }

    SIMDFloat GetBuffer(int x, int y, bool shadow) const
    {
        int width = shadow ? m_ShadowWidth : m_CamWidth;
        const int xAlign = x / SIMDPixel::PIXEL_WIDTH;
        const int yAlign = y / SIMDPixel::PIXEL_HEIGHT;
        const int DepthIndex = yAlign * width + xAlign;
        return shadow ? m_DepthShadowBuffer[DepthIndex] : m_DepthCamBuffer[DepthIndex];
    }

    float GetBufferSingle(int x, int y, bool shadow) const
    {
        int width = shadow ? m_ShadowWidth : m_CamWidth;
        int height = shadow ? m_ShadowHeight : m_CamHeight;
        const int xAlign = x / SIMDPixel::PIXEL_WIDTH;
        const int yAlign = y / SIMDPixel::PIXEL_HEIGHT;
        if (xAlign >= width || yAlign >= height || 0 > xAlign || 0 > yAlign || x < 0 || y < 0)
            return 1.0f;
        const int xR = x % SIMDPixel::PIXEL_WIDTH;
        const int yR = y % SIMDPixel::PIXEL_HEIGHT;
        const int index = yR * SIMDPixel::PIXEL_WIDTH + xR;
        const int DepthIndex = yAlign * width + xAlign;
        if (shadow)
            return m_DepthShadowBuffer[DepthIndex].V[index];

        return m_DepthCamBuffer[DepthIndex].V[index];
    }

    SIMDFloat GetBuffer(int index, bool shadow) const
    {
        assert(0 <= index && index < m_DepthCamBuffer.size());
        assert(0 <= index && index < m_DepthShadowBuffer.size());
        return shadow ? m_DepthShadowBuffer[index] : m_DepthCamBuffer[index];
    }

    void ResetResource() override
    {
        for (auto& depth : m_DepthCamBuffer)
        {
            depth = SIMDFloat(0.0f);
        }
        for (auto& depth : m_DepthShadowBuffer)
        {
            depth = SIMDFloat(0.0f);
        }
    }

  private:
    int m_CamHeight;
    int m_CamWidth;
    int m_ShadowHeight;
    int m_ShadowWidth;
    std::vector<SIMDFloat> m_DepthShadowBuffer;
    std::vector<SIMDFloat> m_DepthCamBuffer;
};
