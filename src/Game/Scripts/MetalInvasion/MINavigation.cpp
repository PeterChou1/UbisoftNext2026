#include "MINavigation.h"

#include "ECSManager.h"
#include "Transform.h"

#include <algorithm>
#include <cmath>
#include <set>

extern ECSManager ECS;

namespace MI
{
    namespace
    {
        constexpr size_t GRID_CELLS = 60;

        Vec3 Flat(const Vec3& v) { return Vec3(v.X, 0.0f, v.Z); }

        Vec3 NormalizedOrZero(Vec3 v)
        {
            float length = v.GetMagnitude();
            return length > 1e-4f ? v * (1.0f / length) : Vec3(0, 0, 0);
        }
    } // namespace

    void FlowField::Build(float halfWidth, float halfHeight)
    {
        m_Field = VectorField{};
        m_Field.SetGridCount(GRID_CELLS, GRID_CELLS);
        m_Field.HalfWidth = halfWidth;
        m_Field.HalfHeight = halfHeight;
        Vec3 origin(0, 0, 0);
        m_Field.CreateVectorField(origin);
        m_CellSize = 2.0f * std::max(halfWidth, halfHeight) / static_cast<float>(GRID_CELLS);
        std::set<Entity> obstacles = ECS.Visit<AIObstacle, Transform>();
        m_Field.SetObstacles(obstacles);
        m_Built = true;
        Refresh();
    }

    void FlowField::AddObstacle(Entity entity)
    {
        if (!m_Built || !ECS.HasComponent<AIObstacle>(entity))
            return;
        std::set<Entity> obstacles{entity};
        m_Field.SetObstacles(obstacles);
    }

    void FlowField::RemoveObstacle(Entity entity)
    {
        if (!m_Built)
            return;
        std::set<Entity> obstacles{entity};
        m_Field.RemoveObstacles(obstacles);
    }

    void FlowField::SetGoal(const Vec3& goal)
    {
        m_Goal = Flat(goal);
        m_HasGoal = true;
        Refresh();
    }

    void FlowField::Refresh()
    {
        if (!m_Built || !m_HasGoal)
            return;
        // Cells the search does not reach must not keep a stale direction
        for (auto& row : m_Field.Map)
        {
            for (MapLoc& cell : row)
            {
                cell.NextX = -1;
                cell.NextY = -1;
            }
        }
        Transform goal(m_Goal);
        m_Field.CalculateVectorField(goal);
    }

    Vec3 FlowField::Direction(const Vec3& from)
    {
        Vec3 toGoal = Flat(m_Goal - from);
        if (!m_Built || !m_HasGoal || toGoal.GetMagnitude() < m_CellSize * 2.0f)
            return NormalizedOrZero(toGoal);

        // The grid is regular: find the cell directly. VectorField::GetLocation
        // (a scan of every cell) is only needed when standing on an obstacle
        const MapLoc& origin = m_Field.Map[0][0];
        int cx = static_cast<int>(std::floor((from.X - origin.Location.X) / m_CellSize));
        int cy = static_cast<int>(std::floor((from.Z - origin.Location.Y) / m_CellSize));
        cx = std::clamp(cx, 0, static_cast<int>(GRID_CELLS) - 1);
        cy = std::clamp(cy, 0, static_cast<int>(GRID_CELLS) - 1);
        const MapLoc* cell = &m_Field.Map[cy][cx];
        if (cell->MapT == MapObstacle)
        {
            Transform here(Flat(from));
            size_t x, y;
            std::tie(x, y) = m_Field.GetLocation(here);
            cell = &m_Field.Map[y][x];
        }
        if (cell->NextX < 0 || cell->NextY < 0)
            return NormalizedOrZero(toGoal);
        // Aim two cells ahead: smoother than steering cell by cell
        const MapLoc* next = &m_Field.Map[cell->NextY][cell->NextX];
        if (next->NextX >= 0 && next->NextY >= 0)
            next = &m_Field.Map[next->NextY][next->NextX];
        Vec3 target(next->Location.X + m_CellSize * 0.5f, 0.0f, next->Location.Y + m_CellSize * 0.5f);
        Vec3 direction = NormalizedOrZero(Flat(target - from));
        return direction.GetMagnitude() > 0.0f ? direction : NormalizedOrZero(toGoal);
    }
} // namespace MI
