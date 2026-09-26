//---------------------------------------------------------------------------------
// PixelBuffer.h
//---------------------------------------------------------------------------------
//
// The Pixel Buffer stores all pixels rasterized during the rasterization stage
// of the pipeline at the end of rasterization the pixels are compared against
// the depth buffer for filtering
//
#pragma once

#include "DepthBuffer.h"
#include "Resource.h"
#include "SIMDPixel.h"

#include <vector>

class PixelBuffer : public Resource
{
  public:
    PixelBuffer(int width, int height)
        : m_Pixelbuffer((width / SIMDPixel::PIXEL_WIDTH) * (height / SIMDPixel::PIXEL_HEIGHT))
        , m_Width(width / SIMDPixel::PIXEL_WIDTH)
    {
    }

    // Add the rasterized pixels of block (x, y); pixels covering the whole
    // block hide the ones added before
    void SetBuffer(int x, int y, const SIMDPixel& pixel, SIMDFloat& mask)
    {
        if (SIMD::All(mask))
            m_Pixelbuffer[y * m_Width + x].clear();
        m_Pixelbuffer[y * m_Width + x].push_back(pixel);
    }

    /**
     * \brief Once pixel has been rasterized accumulate the pixel
     *        so we can distribute the pixels evenly across threads: the
     *        visible ones (that passed the final depth test) are kept
     */
    void AccumulatePixel(const DepthBuffer& depth)
    {
        for (size_t i = 0; i < m_Pixelbuffer.size(); i++)
        {
            for (SIMDPixel& pixel : m_Pixelbuffer[i])
            {
                SIMDFloat mask = pixel.Depth == depth.CameraDepth(static_cast<int>(i));
                if (SIMD::Any(mask))
                {
                    pixel.Mask = mask;
                    m_PixelScreenSpace.push_back(pixel);
                }
            }
        }
    }

    std::vector<SIMDPixel>::iterator begin() { return m_PixelScreenSpace.begin(); }

    std::vector<SIMDPixel>::iterator end() { return m_PixelScreenSpace.end(); }

    void ResetResource() override
    {
        for (auto& pixels : m_Pixelbuffer)
            pixels.clear();
        m_PixelScreenSpace.clear();
    }

  private:
    std::vector<std::vector<SIMDPixel>> m_Pixelbuffer;
    std::vector<SIMDPixel> m_PixelScreenSpace;
    // Blocks per row
    int m_Width{};
};
