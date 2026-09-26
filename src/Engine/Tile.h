//---------------------------------------------------------------------------------
// Tile.h
//---------------------------------------------------------------------------------
//
// Tile is used to segment the display so to allow multi-threaded rendering
// every core is assigned a tile so the rendering pipeline can remain lock free
// and not worry about data races. A tile keeps its triangles in one bin per
// clipping thread
//
#pragma once

#include "Triangle.h"
#include "Vec2.h"

#include <thread>
#include <vector>

static constexpr int TILE_SIZE_X = 32;
static constexpr int TILE_SIZE_Y = 32;

class Tile
{
  public:
    Tile(const Vec2& min, const Vec2& max)
        : m_MinRaster(min)
        , m_MaxRaster(max)
        , m_BinTriangles(std::thread::hardware_concurrency())
    {
    }

    void AddTri(std::uint32_t bin, Triangle& tri) { m_BinTriangles[bin].push_back(tri); }

    void Clear()
    {
        for (auto& binTriangle : m_BinTriangles)
            binTriangle.clear();
    }

    Vec2 GetMin() const { return m_MinRaster; }

    Vec2 GetMax() const { return m_MaxRaster; }

    // Corner 0 .. 3: (min x, min y), (max x, min y), (min x, max y), (max x, max y)
    Vec2 GetCorner(int index) const { return m_MinRaster + CornerIndex[index]; }

    std::vector<std::vector<Triangle>>& GetBinTriangle() { return m_BinTriangles; }

  private:
    Vec2 m_MinRaster;
    Vec2 m_MaxRaster;
    std::vector<std::vector<Triangle>> m_BinTriangles;
    static Vec2 CornerIndex[4];
};
