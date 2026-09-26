//---------------------------------------------------------------------------------
// Map.h
//---------------------------------------------------------------------------------
//
// Grid on the XZ plane for AI navigation: CalculateVectorField stores in every
// cell the next cell on the shortest path to a target, around the cells
// covered by entities with an AIObstacle
//
#pragma once

#include "Transform.h"
#include "Vec2.h"

#include <set>
#include <utility>
#include <vector>

// (x, y) of a grid cell
using Coords = std::pair<size_t, size_t>;

enum MapType
{
    EmptySpace,
    MapObstacle,
    Goal
};

/// Half extents (X, Z) of the area an entity blocks
struct AIObstacle
{
    float Width;
    float Height;
};

struct MapLoc
{
    // World (X, Z) of the cell
    Vec2 Location;
    // Next cell towards the target (-1: none)
    int NextX = -1;
    int NextY = -1;
    MapType MapT = EmptySpace;
};

struct VectorField
{
    /// Nearest free cell to the transform
    Coords GetLocation(Transform& T);

    /// Shortest paths to the target (Dijkstra over the 8 neighbours)
    void CalculateVectorField(Transform& Target);

    /// Build the grid centred on Location
    void CreateVectorField(Vec3& Location);

    void SetObstacles(std::set<Entity>& Obstacles);
    void RemoveObstacles(std::set<Entity>& Obstacles);

    void SetGridCount(size_t Height, size_t Width);

    std::vector<std::vector<MapLoc>> Map;
    // Obstacle entity covering each cell (NULL_ENTITY: none)
    std::vector<std::vector<Entity>> ObstacleTracker;

    // Number of cells along Z / X
    size_t GridCountHeight = 60;
    size_t GridCountWidth = 60;

    // Half size of the grid in world units
    float HalfHeight = 25.0f;
    float HalfWidth = 25.0f;

  private:
    /// Free cells inside the obstacle's box around the transform
    std::vector<Coords> GetLocationObstacle(Transform& T, AIObstacle& obstacle);
};
