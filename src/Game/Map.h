#pragma once

#include "Transform.h"

#include <set>
#include <vector>

using Coords = std::pair<size_t, size_t>;

enum MapType
{
    EmptySpace,
    MapObstacle,
    Goal
};

struct AIObstacle
{
    float Width;
    float Height;
};

struct MapLoc
{
    Vec2 Location;
    int NextX = -1;
    int NextY = -1;
    MapType MapT = EmptySpace;
};

struct VectorField
{

    void ClearField();

    Coords GetLocation(Transform& T);

    void CalculateVectorField(Transform& Target);

    void CreateVectorField(Vec3& Location);

    void SetObstacles(std::set<Entity>& Obstacles);

    void RemoveObstacles(std::set<Entity>& Obstacles);

    void SetDimension(float Height, float Width);

    void SetGridCount(size_t Height, size_t Width);

    Vec3 LocationVectorField;

    std::vector<std::vector<MapLoc>> Map;
    std::vector<std::vector<Entity>> ObstacleTracker;

    std::vector<Coords> GetLocationObstacle(Transform& T, AIObstacle& obstacle);

    // Grid Count Represent how "dense" you want your vector field to be
    size_t GridCountHeight = 60;
    size_t GridCountWidth = 60;

    // Height and Width of the vector field in the world space
    float HalfHeight = 25.0f;
    float HalfWidth = 25.0f;
};