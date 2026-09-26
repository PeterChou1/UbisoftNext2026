#include "Tiles.h"

#include "stdafx.h"

#include <algorithm>

namespace
{
    // Tiles of TILE_SIZE_X x TILE_SIZE_Y covering width x height, row by row
    std::vector<Tile> MakeTiles(int width, int height)
    {
        std::vector<Tile> tiles;
        for (int y = 0; y < height / TILE_SIZE_Y; ++y)
        {
            for (int x = 0; x < width / TILE_SIZE_X; ++x)
            {
                Vec2 max(static_cast<float>((std::min)((x + 1) * TILE_SIZE_X, width)),
                         static_cast<float>((std::min)((y + 1) * TILE_SIZE_Y, height)));
                Vec2 min(static_cast<float>((std::min)(x * TILE_SIZE_X, width)),
                         static_cast<float>((std::min)(y * TILE_SIZE_Y, height)));
                tiles.emplace_back(min, max);
            }
        }
        return tiles;
    }
} // namespace

Tiles::Tiles(int width, int height, int shadowWidth, int shadowHeight)
    : TilesArray(MakeTiles(width, height))
    , ShadowTilesArray(MakeTiles(shadowWidth, shadowHeight))
    , TileCountX(width / TILE_SIZE_X)
    , ShadowTileCountX(shadowWidth / TILE_SIZE_X)
{
}
