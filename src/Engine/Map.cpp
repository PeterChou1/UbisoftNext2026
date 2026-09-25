#include "Map.h"

#include "ECSManager.h"
#include "stdafx.h"

extern ECSManager ECS;

// A small struct to represent a node in the priority queue.
struct NodeDistance
{
    size_t index;
    float distance;
    bool operator>(const NodeDistance& other) const { return distance > other.distance; }
};

Coords VectorField::GetLocation(Transform& T)
{
    Coords Location;
    float Distance = std::numeric_limits<float>::max();
    size_t Height = Map.size();
    size_t Width = Map[0].size();
    for (size_t Y = 0; Y < Height; Y++)
    {
        for (size_t X = 0; X < Width; X++)
        {
            MapLoc& Loc = Map[Y][X];
            if (Loc.MapT == MapObstacle)
                continue;

            float CX = std::abs(T.GetWorldPosition().X - Loc.Location.X);
            float CY = std::abs(T.GetWorldPosition().Z - Loc.Location.Y);
            float CurrentDist = CX + CY;
            if (CurrentDist < Distance)
            {
                Location.first = X;
                Location.second = Y;
                Distance = CurrentDist;
            }
        }
    }
    return Location;
}

void VectorField::CalculateVectorField(Transform& Target)
{
    size_t startX, startY;
    std::tie(startX, startY) = GetLocation(Target);

    size_t Height = Map.size();
    if (Height == 0)
        return;

    size_t Width = Map[0].size();
    size_t GridSize = Height * Width;

    // Prepare arrays for Dijkstra
    std::vector<bool> Visited(GridSize, false);
    std::vector<float> Distance(GridSize, std::numeric_limits<float>::max());

    // 8 possible directions
    std::pair<int, int> Paths[8] = {
            {1, 1}, {1, 0}, {1, -1}, {0, 1}, {0, -1}, {-1, 1}, {-1, 0}, {-1, -1}};

    // Convert (x, y) to linear index
    auto toIndex = [&](size_t x, size_t y) { return y * Width + x; };

    // Convert linear index back to (x, y)
    auto toCoord = [&](size_t idx) { return std::make_pair(idx % Width, idx / Width); };
    // Set the distance to start cell as 0
    Distance[toIndex(startX, startY)] = 0.0f;

    // Priority queue for Dijkstra (min-heap by distance)
    std::priority_queue<NodeDistance, std::vector<NodeDistance>, std::greater<NodeDistance>> pq;

    // Push the start node
    pq.push({toIndex(startX, startY), 0.0f});

    while (!pq.empty())
    {
        // Pop the cell with the smallest distance
        NodeDistance top = pq.top();
        pq.pop();

        size_t idx = top.index;

        // If we've already visited it, skip
        if (Visited[idx])
            continue;
        Visited[idx] = true;

        // If distance is inf, no path from start -> idx
        if (Distance[idx] == std::numeric_limits<float>::max())
        {
            break;
        }

        // Get (x, y) for this index
        size_t x, y;
        std::tie(x, y) = toCoord(idx);

        // Explore neighbors
        for (auto& dir : Paths)
        {
            int nx = static_cast<int>(x) + dir.first;
            int ny = static_cast<int>(y) + dir.second;

            // Check bounds
            if (nx < 0 || ny < 0 || nx >= static_cast<int>(Width) || ny >= static_cast<int>(Height))
            {
                continue;
            }

            size_t nxu = static_cast<size_t>(nx);
            size_t nyu = static_cast<size_t>(ny);
            size_t nIndex = toIndex(nxu, nyu);

            // If visited or obstacle, skip
            MapLoc& Adjacent = Map[nyu][nxu];
            if (Adjacent.MapT == MapObstacle || Visited[nIndex])
            {
                continue;
            }

            // Current cell is Board->Field.Map[y][x]
            MapLoc& Current = Map[y][x];

            // Calculate new distance
            Vec2 diff = Current.Location - Adjacent.Location;
            float newDist =
                    Distance[idx] + diff.GetMagnitude(); // or 1.414f if uniform diagonals, etc.

            // If found a shorter path to neighbor
            if (newDist < Distance[nIndex])
            {
                Distance[nIndex] = newDist;
                // Store "parent" pointer or coordinate so we can reconstruct or use NextX / NextY
                Adjacent.NextX = x;
                Adjacent.NextY = y;
                // Push updated distance into the queue
                pq.push({nIndex, newDist});
            }
        }
    }
}

void VectorField::CreateVectorField(Vec3& Location)
{
    LocationVectorField = Location;

    float StartX = Location.X - HalfWidth;
    float StartY = Location.Z - HalfHeight;
    float GridWidth = HalfWidth * 2.0f / static_cast<float>(GridCountWidth);
    float GridHeight = HalfHeight * 2.0f / static_cast<float>(GridCountHeight);
    float CurY = StartY;

    for (size_t Y = 0; Y < GridCountHeight; Y++)
    {
        float CurX = StartX;
        std::vector<MapLoc> Row;
        std::vector<Entity> Track;
        for (size_t X = 0; X < GridCountWidth; X++)
        {
            Vec2 Start = Vec2(CurX, CurY);
            Row.push_back({Start, 0, 0, EmptySpace});
            Track.push_back(NULL_ENTITY);
            CurX += GridWidth;
        }
        ObstacleTracker.push_back(Track);
        Map.push_back(Row);
        CurY += GridHeight;
    }
}

void VectorField::SetObstacles(std::set<Entity>& Obstacles)
{
    for (Entity e : Obstacles)
    {
        AIObstacle& obstacle = ECS.GetComponent<AIObstacle>(e);
        Transform& t = ECS.GetComponent<Transform>(e);
        std::vector<Coords> Locations = GetLocationObstacle(t, obstacle);
        for (const Coords c : Locations)
        {
            const float X = c.first;
            const float Y = c.second;
            ObstacleTracker[Y][X] = e;
            Map[Y][X].MapT = MapObstacle;
            Map[Y][X].NextX = -1;
            Map[Y][X].NextY = -1;
        }
    }
}

void VectorField::RemoveObstacles(std::set<Entity>& Obstacles)
{
    for (Entity e : Obstacles)
    {
        for (size_t Y = 0; Y < GridCountHeight; Y++)
        {
            for (size_t X = 0; X < GridCountWidth; X++)
            {
                if (ObstacleTracker[Y][X] == e)
                {
                    Map[Y][X].MapT = EmptySpace;
                    Map[Y][X].NextX = -1;
                    Map[Y][X].NextY = -1;
                    ObstacleTracker[Y][X] = NULL_ENTITY;
                }
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
    std::vector<Coords> Locations;
    size_t Height = Map.size();
    size_t Width = Map[0].size();
    float CurX = T.GetWorldPosition().X;
    float CurZ = T.GetWorldPosition().Z;

    Vec2 maxPoint = Vec2(CurX + obstacle.Width, CurZ + obstacle.Height);

    Vec2 minPoint = Vec2(CurX - obstacle.Width, CurZ - obstacle.Height);

    for (size_t Y = 0; Y < Height; Y++)
    {
        for (size_t X = 0; X < Width; X++)
        {
            MapLoc& Loc = Map[Y][X];
            if (Loc.MapT == MapObstacle)
                continue;

            if (Loc.Location.X <= maxPoint.X && minPoint.X <= Loc.Location.X &&
                Loc.Location.Y <= maxPoint.Y && minPoint.Y <= Loc.Location.Y)
            {
                Locations.push_back({X, Y});
            }
        }
    }

    return Locations;
}
