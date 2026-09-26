#include "Map.h"

#include "ECSManager.h"

#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>

extern ECSManager ECS;

namespace
{
    // A cell (linear index) in the Dijkstra priority queue
    struct NodeDistance
    {
        size_t index;
        float distance;
        bool operator>(const NodeDistance& other) const { return distance > other.distance; }
    };
} // namespace

Coords VectorField::GetLocation(Transform& T)
{
    const Vec3 position = T.GetWorldPosition();
    Coords location;
    float distance = std::numeric_limits<float>::max();
    for (size_t y = 0; y < Map.size(); y++)
    {
        for (size_t x = 0; x < Map[0].size(); x++)
        {
            const MapLoc& loc = Map[y][x];
            if (loc.MapT == MapObstacle)
                continue;

            // Manhattan distance
            float current =
                    std::abs(position.X - loc.Location.X) + std::abs(position.Z - loc.Location.Y);
            if (current < distance)
            {
                location = {x, y};
                distance = current;
            }
        }
    }
    return location;
}

void VectorField::CalculateVectorField(Transform& Target)
{
    const size_t height = Map.size();
    if (height == 0)
        return;
    const size_t width = Map[0].size();

    size_t startX, startY;
    std::tie(startX, startY) = GetLocation(Target);

    std::vector<bool> visited(height * width, false);
    std::vector<float> distance(height * width, std::numeric_limits<float>::max());

    const std::pair<int, int> NEIGHBOURS[8] = {
            {1, 1}, {1, 0}, {1, -1}, {0, 1}, {0, -1}, {-1, 1}, {-1, 0}, {-1, -1}};

    auto toIndex = [&](size_t x, size_t y) { return y * width + x; };

    // Min-heap by distance
    std::priority_queue<NodeDistance, std::vector<NodeDistance>, std::greater<NodeDistance>> queue;
    distance[toIndex(startX, startY)] = 0.0f;
    queue.push({toIndex(startX, startY), 0.0f});

    while (!queue.empty())
    {
        const size_t idx = queue.top().index;
        queue.pop();

        if (visited[idx])
            continue;
        visited[idx] = true;

        // Unreachable
        if (distance[idx] == std::numeric_limits<float>::max())
            break;

        const size_t x = idx % width;
        const size_t y = idx / width;
        const MapLoc& current = Map[y][x];

        for (const auto& dir : NEIGHBOURS)
        {
            const int nx = static_cast<int>(x) + dir.first;
            const int ny = static_cast<int>(y) + dir.second;
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height))
                continue;

            const size_t nIndex = toIndex(nx, ny);
            MapLoc& adjacent = Map[ny][nx];
            if (adjacent.MapT == MapObstacle || visited[nIndex])
                continue;

            const float newDistance =
                    distance[idx] + (current.Location - adjacent.Location).GetMagnitude();
            if (newDistance < distance[nIndex])
            {
                distance[nIndex] = newDistance;
                adjacent.NextX = static_cast<int>(x);
                adjacent.NextY = static_cast<int>(y);
                queue.push({nIndex, newDistance});
            }
        }
    }
}

void VectorField::CreateVectorField(Vec3& Location)
{
    const float startX = Location.X - HalfWidth;
    const float cellWidth = HalfWidth * 2.0f / static_cast<float>(GridCountWidth);
    const float cellHeight = HalfHeight * 2.0f / static_cast<float>(GridCountHeight);
    float curY = Location.Z - HalfHeight;

    for (size_t y = 0; y < GridCountHeight; y++)
    {
        float curX = startX;
        std::vector<MapLoc> row;
        for (size_t x = 0; x < GridCountWidth; x++)
        {
            row.push_back({Vec2(curX, curY), 0, 0, EmptySpace});
            curX += cellWidth;
        }
        ObstacleTracker.push_back(std::vector<Entity>(GridCountWidth, NULL_ENTITY));
        Map.push_back(row);
        curY += cellHeight;
    }
}

void VectorField::SetObstacles(std::set<Entity>& Obstacles)
{
    for (Entity e : Obstacles)
    {
        AIObstacle& obstacle = ECS.GetComponent<AIObstacle>(e);
        Transform& t = ECS.GetComponent<Transform>(e);
        for (const Coords& c : GetLocationObstacle(t, obstacle))
        {
            const size_t x = c.first;
            const size_t y = c.second;
            ObstacleTracker[y][x] = e;
            Map[y][x].MapT = MapObstacle;
            Map[y][x].NextX = -1;
            Map[y][x].NextY = -1;
        }
    }
}

void VectorField::RemoveObstacles(std::set<Entity>& Obstacles)
{
    for (Entity e : Obstacles)
    {
        for (size_t y = 0; y < GridCountHeight; y++)
        {
            for (size_t x = 0; x < GridCountWidth; x++)
            {
                if (ObstacleTracker[y][x] != e)
                    continue;
                Map[y][x].MapT = EmptySpace;
                Map[y][x].NextX = -1;
                Map[y][x].NextY = -1;
                ObstacleTracker[y][x] = NULL_ENTITY;
            }
        }
    }
}

void VectorField::SetGridCount(size_t Height, size_t Width)
{
    GridCountWidth = Width;
    GridCountHeight = Height;
}

std::vector<Coords> VectorField::GetLocationObstacle(Transform& T, AIObstacle& obstacle)
{
    assert(!Map.empty() && "Create Vector Field must be called first");
    const Vec3 position = T.GetWorldPosition();
    const Vec2 maxPoint(position.X + obstacle.Width, position.Z + obstacle.Height);
    const Vec2 minPoint(position.X - obstacle.Width, position.Z - obstacle.Height);

    std::vector<Coords> locations;
    for (size_t y = 0; y < Map.size(); y++)
    {
        for (size_t x = 0; x < Map[0].size(); x++)
        {
            const MapLoc& loc = Map[y][x];
            if (loc.MapT == MapObstacle)
                continue;

            if (loc.Location.X <= maxPoint.X && minPoint.X <= loc.Location.X &&
                loc.Location.Y <= maxPoint.Y && minPoint.Y <= loc.Location.Y)
                locations.push_back({x, y});
        }
    }
    return locations;
}
