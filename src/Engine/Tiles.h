//---------------------------------------------------------------------------------
// Tiles.h
//---------------------------------------------------------------------------------
//
// Container classes for all tiles see Tile.h for more information
//
#pragma once
#include "Resource.h"
#include "Tile.h"

class Tiles : public Resource
{
  public:
    Tiles(int width, int height, int widthS, int heightS);

    void ResetResource() override
    {
        for (int i = 0; i < TilesArray.size(); i++)
            TilesArray[i].Clear();
        for (int i = 0; i < ShadowTilesArray.size(); i++)
            ShadowTilesArray[i].Clear();
    }

    void SetTiles(int width, int height);

    void SetShadowTiles(int widthS, int heightS);

    std::vector<Tile> TilesArray;
    std::vector<Tile> ShadowTilesArray;
    int TILE_COUNT_X;
    int TILE_COUNT_Y;
    int TILE_S_COUNT_X;
    int TILE_S_COUNT_Y;
};
