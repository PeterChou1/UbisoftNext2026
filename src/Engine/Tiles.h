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
    // The camera's tiles cover width x height, the shadow map's
    // shadowWidth x shadowHeight
    Tiles(int width, int height, int shadowWidth, int shadowHeight);

    void ResetResource() override
    {
        for (Tile& tile : TilesArray)
            tile.Clear();
        for (Tile& tile : ShadowTilesArray)
            tile.Clear();
    }

    std::vector<Tile> TilesArray;
    std::vector<Tile> ShadowTilesArray;
    // Tiles per row
    int TileCountX;
    int ShadowTileCountX;
};
