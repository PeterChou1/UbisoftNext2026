//---------------------------------------------------------------------------------
// Rasterizer.h
//---------------------------------------------------------------------------------
//
// Tile based multi-threaded Rasterizer using AVX2 instruction
// Rasterization algorithm is based off Larrabee Rasterization
// see here:
// https://www.cs.cmu.edu/afs/cs/academic/class/15869-f11/www/readings/abrash09_lrbrast.pdf
//
#pragma once

#include "Camera.h"
#include "ClippedTriangleBuffer.h"
#include "GameOptions.h"
#include "Lighting.h"
#include "PixelBuffer.h"
#include "RenderConstants.h"
#include "Tiles.h"

class RasterizerSystem
{
  public:
    RasterizerSystem();

    void Rasterize();

  private:
    void
    RasterizeTriangle(Triangle& tri, Vec2& tileMin, Vec2& tileMax, bool shadows, bool perspective);

    void AssignTriangle(Triangle& tri, std::vector<Tile>& tiles, unsigned int binID, bool shadow);

    void RenderLine();

    void AssignTile();

    void RasterizeTiles();

    std::shared_ptr<Lighting> m_Lighting;
    std::shared_ptr<Camera> m_Camera;
    std::shared_ptr<ClippedTriangleBuffer> m_ClippedTriangle;
    std::shared_ptr<Tiles> m_Tiles;
    std::shared_ptr<PixelBuffer> m_PixelBuffer;
    std::shared_ptr<DepthBuffer> m_DepthBuffer;
    std::shared_ptr<RenderConstants> m_RenderConstants;
    std::shared_ptr<GameOptions> m_GameOptions;
};
