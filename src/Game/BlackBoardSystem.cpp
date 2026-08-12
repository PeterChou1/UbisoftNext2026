#define NOMINMAX
#include "BlackBoardSystem.h"

#include "Camera.h"
#include "Concurrent.h"
#include "ECSManager.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

size_t minDistance(size_t Size, std::vector<float> dist, std::vector<bool> processed)
{
    float min = std::numeric_limits<float>::max();
    size_t index = 0;
    for (size_t i = 0; i < Size; i++)
    {
        if (processed[i] == false && dist[i] <= min)
        {
            min = dist[i];
            index = i;
        }
    }
    return index;
}

BlackBoardSystem::BlackBoardSystem()
{
    Board = ECS.GetResource<BlackBoard>();
}

void BlackBoardSystem::Update(float deltaTime)
{
    if (Board->UnitTarget == NULL_ENTITY || !ECS.HasComponent<Transform>(Board->UnitTarget))
    {
        return;
    }
    Board->DeltaTime = deltaTime;
}

void BlackBoardSystem::Render()
{
    // Used for debugging renders all pathfinding paths
    std::vector<Vertex> debugVertexBuffer;

    for (std::vector<MapLoc>& row : Board->UnitVectorField.Map)
    {
        for (MapLoc& col : row)
        {
            float X = col.Location.X;
            float Y = col.Location.Y;

            if (col.NextY == -1 && col.NextX == -1)
                continue;

            MapLoc& Next = Board->UnitVectorField.Map[col.NextY][col.NextX];

            float NextX = Next.Location.X;
            float NextY = Next.Location.Y;

            Vertex point1 = Vec3(X, 0.0f, Y);
            Vertex point2 = Vec3(NextX, 0.0f, NextY);
            if (col.MapT == Goal)
            {
                point1.Color = Vec3(1, 0, 0);
                point2.Color = Vec3(1, 0, 0);
            }
            else
            {
                point1.Color = Vec3(0, 1, 0);
                point2.Color = Vec3(0, 1, 0);
            }
            debugVertexBuffer.push_back(point1);
            debugVertexBuffer.push_back(point2);
        }
    }
    auto m_Cam = ECS.GetResource<Camera>();

    Concurrent::ForEach(debugVertexBuffer.begin(), debugVertexBuffer.end(), [&](Vertex& v) {
        v.Projection = m_Cam->Proj * Vec4(m_Cam->WorldToCamera(v.Position));
    });

    int lineSize = static_cast<int>(debugVertexBuffer.size() / 2);

    for (int i = 0; i < lineSize; i++)
    {
        int idx = i * 2;

        Vertex& start = debugVertexBuffer[idx];
        Vertex& end = debugVertexBuffer[idx + 1];

        start.PerspectiveDivision();
        end.PerspectiveDivision();

        m_Cam->ToRasterSpaceUnclamped(start.Projection);
        m_Cam->ToRasterSpaceUnclamped(end.Projection);

        float startX = start.Projection.X;
        float startY = start.Projection.Y;

        float endX = end.Projection.X;
        float endY = end.Projection.Y;

        Vec3 color = start.Color;
        App::DrawLine(startX, startY, endX, endY, 0.0, 0.0, 0.0);
    }
    float top = 500;
    float cur = 0;
    for (auto kv : Board->BehaviorTreeDataBase)
    {
        App::Print(40, top + cur, kv.second->GetRunning().c_str(), 1.0, 0.0, 1.0);
        cur += 20.0f;
    }
}
