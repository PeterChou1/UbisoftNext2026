#define NOMINMAX
#include "BehaviorTree.h"

#include "BlackBoard.h"
#include "ECSManager.h"
#include "Transform.h"
#include "app.h"
#include "stdafx.h"

#include <GameUtils.h>
#include <random>
#include <unordered_set>

extern ECSManager ECS;

Status VectorFieldNode::Process()
{

    auto Board = ECS.GetResource<BlackBoard>();
    Transform& T = ECS.GetComponent<Transform>(WalkEntity);
    Transform& PlayerT = ECS.GetComponent<Transform>(Board->UnitTarget);
    Vec3 CurLoc = T.GetWorldPosition();
    size_t LocX, LocY;
    if (PrevLoc == nullptr)
    {
        std::tie(LocX, LocY) = Field.GetLocation(T);
        MapLoc& Loc = Field.Map[LocY][LocX];
        PrevLoc = &Loc;
    }
    Vec3 PrevVec3 = Vec3(PrevLoc->Location.X, CurLoc.Y, PrevLoc->Location.Y);
    // if vector is too great
    if ((PrevVec3 - CurLoc).GetMagnitude() > TooFarThreshold)
    {
        std::tie(LocX, LocY) = Field.GetLocation(T);
        MapLoc& Loc = Field.Map[LocY][LocX];
        PrevLoc = &Loc;
    }

    if (PrevLoc->NextX == -1 || PrevLoc->NextY == -1)
        return Failure;

    Vec3 GoalLoc = PlayerT.GetWorldPosition();
    if ((GoalLoc - CurLoc).GetMagnitude() < StopDistance)
        return Success;

    MapLoc& NextLoc = Field.Map[PrevLoc->NextY][PrevLoc->NextX];
    Vec3 NextNode = Vec3(NextLoc.Location.X, CurLoc.Y, NextLoc.Location.Y);
    float DistanceToNext = (NextNode - CurLoc).GetMagnitude();
    if (DistanceToNext < 0.1)
    {
        PrevLoc = &NextLoc;
        NextLoc = Field.Map[PrevLoc->NextY][PrevLoc->NextX];
    }

    Vec2 Next = NextLoc.Location - Vec2(CurLoc.X, CurLoc.Z);
    Next.Normalize();
    Vec3 Delta = Vec3(Next.X, 0, Next.Y) * Speed * Board->DeltaTime;

    SmoothInterpolate(Delta, T, TurnSpeed, Board->DeltaTime);
    return Running;
}

Status SelectRandomPatrolPoint::Process()
{
    if (Field.Map.empty() || Field.Map[0].empty())
        return Failure;

    Transform& T = ECS.GetComponent<Transform>(WalkEntity);
    Vec3 CurPos = T.GetWorldPosition();

    if (PotentialLocations.empty())
    {
        size_t Height = Field.Map.size();
        size_t Width = Field.Map[0].size();
        // Gather valid map locations
        for (size_t y = 0; y < Height; y++)
        {
            for (size_t x = 0; x < Width; x++)
            {
                MapLoc& Loc = Field.Map[y][x];
                if (Loc.MapT == EmptySpace && AABBPoint(PatrolBox, Loc.Location.X, Loc.Location.Y))
                    PotentialLocations.push_back(&Loc);
            }
        }
    }

    if (PotentialLocations.empty())
        return Failure;

    Vec3 Target;
    float CurrDist2Map;
    do
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, PotentialLocations.size() - 1);
        unsigned int index = static_cast<unsigned int>(dist(gen));
        MapLoc* loc = PotentialLocations[index];
        Target = {loc->Location.X, CurPos.Y, loc->Location.Y};
        CurrDist2Map = (Target - CurPos).GetMagnitude();
    } while (CurrDist2Map < Distance);

    auto Board = ECS.GetResource<BlackBoard>();
    Board->PatrolTargets[WalkEntity] = Target;
    return Success;
}

void SelectRandomPatrolPoint::Clear()
{
    PotentialLocations.clear();
    Transform& T = ECS.GetComponent<Transform>(WalkEntity);
    Vec3 CurPos = T.GetWorldPosition();
    Vec2 Pos = Vec2(CurPos.X, CurPos.Z);
    Vec2 Width = (PatrolBox.OriginalMax - PatrolBox.OriginalMin) / 2;
    PatrolBox = AABB({Pos + Width, Pos - Width});
}

// A simple structure for open/closed nodes
struct NodeRecord
{
    Coords coords;
    float gCost; // cost from start
    float hCost; // heuristic cost to end
    float fCost() const { return gCost + hCost; }
    Coords parent; // to reconstruct path
};

std::vector<MapLoc> ComputePathAStar(VectorField& field, Coords start, Coords goal)
{
    // 1) Initialize open/closed sets
    auto cmp = [](const NodeRecord& a, const NodeRecord& b) { return a.fCost() > b.fCost(); };
    std::priority_queue<NodeRecord, std::vector<NodeRecord>, decltype(cmp)> open(cmp);

    std::unordered_map<size_t, NodeRecord> visited;
    std::unordered_set<size_t> closedSet;

    auto toKey = [&](size_t x, size_t y) { return x + (y * field.Map[0].size()); };

    // 2) Push start node
    NodeRecord startNode{start, 0.0f, 0.0f, start};
    open.push(startNode);
    visited[toKey(start.first, start.second)] = startNode;

    // 3) Basic A* (4 directions)
    const int dirX[4] = {1, -1, 0, 0};
    const int dirY[4] = {0, 0, 1, -1};

    while (!open.empty())
    {
        NodeRecord current = open.top();
        open.pop();

        // Potentially skip if we already have a better path
        size_t currentKey = toKey(current.coords.first, current.coords.second);
        auto currentIt = visited.find(currentKey);
        if (currentIt == visited.end())
        {
            // Should not happen in typical usage
            continue;
        }
        // If we already found a better route to 'current', skip this one
        if (currentIt->second.fCost() < current.fCost())
        {
            continue;
        }

        // If we've reached the goal, reconstruct path
        if (current.coords == goal)
        {
            std::vector<MapLoc> path;
            // Climb backwards from 'current' to 'start'
            while (current.coords != start)
            {
                path.push_back(field.Map[current.coords.second][current.coords.first]);

                // Move to the parent
                Coords parentCoords = current.parent;
                current = visited[toKey(parentCoords.first, parentCoords.second)];
            }

            // Add the start node itself
            path.push_back(field.Map[start.second][start.first]);

            // Reverse to get path from start -> goal
            std::reverse(path.begin(), path.end());
            return path;
        }

        // If it's in the closed set, skip
        if (closedSet.find(currentKey) != closedSet.end())
        {
            continue;
        }
        // Otherwise mark it as closed
        closedSet.insert(currentKey);

        // Check neighbors
        for (int i = 0; i < 4; i++)
        {
            int nx = static_cast<int>(current.coords.first) + dirX[i];
            int ny = static_cast<int>(current.coords.second) + dirY[i];

            // Check boundaries & obstacles
            if (ny < 0 || ny >= static_cast<int>(field.Map.size()))
                continue;
            if (nx < 0 || nx >= static_cast<int>(field.Map[0].size()))
                continue;
            if (field.Map[ny][nx].MapT == MapObstacle)
                continue;

            // If neighbor is already closed, skip
            size_t neighborKey = toKey(nx, ny);
            if (closedSet.find(neighborKey) != closedSet.end())
            {
                continue;
            }

            // Compute costs
            float newG = current.gCost + 1.0f;
            float h = std::abs((float)goal.first - nx) + std::abs((float)goal.second - ny);
            float newF = newG + h;

            auto it = visited.find(neighborKey);
            // If we never visited neighbor or we found a cheaper cost
            if (it == visited.end() || newF < it->second.fCost())
            {
                NodeRecord neighbor;
                neighbor.coords = {(size_t)nx, (size_t)ny};
                neighbor.gCost = newG;
                neighbor.hCost = h;
                neighbor.parent = current.coords;

                visited[neighborKey] = neighbor;
                open.push(neighbor);
            }
        }
    }

    // No path found
    return {};
}

Status NavigateToPatrolPoint::Process()
{
    auto Board = ECS.GetResource<BlackBoard>();
    if (Board->PatrolTargets.count(WalkEntity) == 0)
        return Failure;

    TargetPos = Board->PatrolTargets[WalkEntity];
    return NavigateToPoint::Process();
}

Status Wait::Process()
{
    const auto Board = ECS.GetResource<BlackBoard>();
    if (CurrentTime < 0.0)
    {
        CurrentTime = Cooldown;
        return Running;
    }
    if (CurrentTime == 0.0f)
    {
        CurrentTime = -1.0f;
        return Success;
    }
    CurrentTime -= Board->DeltaTime / 1000;
    CurrentTime = std::max(0.0f, CurrentTime);
    return Running;
}

Status NavigateToPoint::Process()
{
    auto Board = ECS.GetResource<BlackBoard>();
    Transform& transform = ECS.GetComponent<Transform>(WalkEntity);
    // Target position we want to reach
    Vec3 currentPos = transform.GetWorldPosition();

    // 1) If we have no path yet or target can move
    //    Compute Path
    if (PatrolPath.empty())
    {
        PatrolPath.clear();
        // Convert current world position to grid coords
        Coords startCoords = Field.GetLocation(transform);
        // Convert the patrol target position to grid coords
        // (You could do something like: Field.GetLocationForPos(targetPos))
        Transform EndT = Transform(TargetPos);
        Coords endCoords = Field.GetLocation(EndT);

        // Compute path (A* or BFS)
        // This function should return a list of MapLoc
        PatrolPath = ComputePathAStar(Field, startCoords, endCoords);

        // If no path was found, we fail
        if (PatrolPath.empty())
            return Failure;
    }
    // Next node (in world coords) we want to move toward
    MapLoc& nextLoc = PatrolPath.front();

    // Convert grid coords back to world position
    Vec3 nextWorldPos(nextLoc.Location.X, currentPos.Y, nextLoc.Location.Y);

    // Direction from current to next
    Vec3 direction = nextWorldPos - currentPos;
    float distance = direction.GetMagnitude();
    // We're too far from the next location re-derive another path
    if (distance > tooFarThreshold)
    {
        PatrolPath.clear();
        return Running;
    }

    if (distance < closeEnoughThreshold)
    {
        // We are effectively at the first waypoint
        PatrolPath.erase(PatrolPath.begin());

        // If that was the last waypoint, we have arrived
        if (PatrolPath.empty())
            return Success;
    }
    // Move toward next waypoint
    direction.Normalize();
    Vec3 Delta = direction * moveSpeed * Board->DeltaTime;
    SmoothInterpolate(Delta, transform, TurnSpeed, Board->DeltaTime);
    // If we still have more waypoints, we keep running
    return Running;
}

Status ExecuteChildInTime::Process()
{
    auto Board = ECS.GetResource<BlackBoard>();
    if (CurrentTime < 0.0f)
    {
        CurrentTime = Time;
    }

    if (CurrentTime == 0.0f)
    {
        CurrentTime = -1.0f;
        return Success;
    }

    if (!Children.empty())
        Children[0]->Process();

    CurrentTime -= Board->DeltaTime / 1000;
    CurrentTime = std::max(0.0f, CurrentTime);

    return Running;
}

Status PlayVoiceLine::Process()
{
    App::PlayAudio(VoiceLine.c_str());
    return Success;
}
