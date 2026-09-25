#include "Tiles.h"

#include "stdafx.h"

Tiles::Tiles(int width, int height, int widthS, int heightS)
{
    TILE_COUNT_X = width / TILE_SIZE_X;
    TILE_COUNT_Y = height / TILE_SIZE_Y;
    TILE_S_COUNT_X = widthS / TILE_SIZE_X;
    TILE_S_COUNT_Y = heightS / TILE_SIZE_Y;

    for (int y = 0; y < TILE_COUNT_Y; ++y)
    {
        for (int x = 0; x < TILE_COUNT_X; ++x)
        {
            auto max = Vec2(static_cast<float>((std::min)((x + 1) * TILE_SIZE_X, width)),
                            static_cast<float>((std::min)((y + 1) * TILE_SIZE_Y, height)));
            auto min = Vec2(static_cast<float>((std::min)(x * TILE_SIZE_X, width)),
                            static_cast<float>((std::min)(y * TILE_SIZE_Y, height)));
            auto t = Tile(min, max);
            TilesArray.push_back(t);
        }
    }
    for (int y = 0; y < TILE_S_COUNT_Y; ++y)
    {
        for (int x = 0; x < TILE_S_COUNT_X; ++x)
        {
            auto max = Vec2(static_cast<float>((std::min)((x + 1) * TILE_SIZE_X, widthS)),
                            static_cast<float>((std::min)((y + 1) * TILE_SIZE_Y, heightS)));
            auto min = Vec2(static_cast<float>((std::min)(x * TILE_SIZE_X, widthS)),
                            static_cast<float>((std::min)(y * TILE_SIZE_Y, heightS)));
            auto t = Tile(min, max);
            ShadowTilesArray.push_back(t);
        }
    }
}
