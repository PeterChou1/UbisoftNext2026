//---------------------------------------------------------------------------------
// ColorBuffer.h
//---------------------------------------------------------------------------------
//
// The ColorBuffer is used to store all the pixel that the fragment shader
// outputs
//
#pragma once

#include "Concurrent.h"
#include "Resource.h"

#include <cassert>
#include <vector>

class ColorBuffer : public Resource
{
  public:
    ColorBuffer(int width, int height)
        : m_Height(height)
        , m_Width(width)
    {
        m_Buffer.resize((width + 1) * (height + 1) * 3);
    }

    void SetColor(int x, int y, unsigned char r, unsigned char g, unsigned char b)
    {
        assert(0 <= x && x <= m_Width && 0 <= y && y <= m_Height && "out of bounds index");
        const int i = (y * m_Width + x) * 3;
        m_Buffer[i] = r;
        m_Buffer[i + 1] = g;
        m_Buffer[i + 2] = b;
    }

    void GetColor(int x, int y, unsigned char& r, unsigned char& g, unsigned char& b) const
    {
        const int i = (y * m_Width + x) * 3;
        r = m_Buffer[i];
        g = m_Buffer[i + 1];
        b = m_Buffer[i + 2];
    }

    void ResetResource() override
    {
        Concurrent::ForEach(
                m_Buffer.begin(), m_Buffer.end(), [](unsigned char& color) { color = 0; });
    }

  private:
    int m_Height{};
    int m_Width{};
    std::vector<unsigned char> m_Buffer;
};
