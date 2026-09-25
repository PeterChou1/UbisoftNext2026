//---------------------------------------------------------------------------------
// MINavigation.h
//---------------------------------------------------------------------------------
//
// Flow field path finding for Metal Invasion, a thin wrapper around the
// engine's VectorField (Map.h). Every scene object with an AIObstacle
// component blocks its cells (base, crystals, walls); SetGoal runs Dijkstra
// from the goal once, then any number of units ask Direction(position) for
// the way to go. The original game used one field for the enemies (goal =
// the base) and one for the player's units (goal = the last move order).
//
#pragma once

#include "Entity.h"
#include "Map.h"
#include "Vec3.h"

namespace MI
{
    class FlowField
    {
      public:
        /**
         * \brief Grid covering [-halfWidth, halfWidth] x [-halfHeight, halfHeight],
         *        blocked by every entity with an AIObstacle
         */
        void Build(float halfWidth, float halfHeight);

        void AddObstacle(Entity entity);
        void RemoveObstacle(Entity entity);

        /**
         * \brief New destination (recomputes the whole field)
         */
        void SetGoal(const Vec3& goal);

        /**
         * \brief Recompute for the current goal (after obstacles changed)
         */
        void Refresh();

        /**
         * \brief Unit direction to walk from `from` towards the goal, going
         *        around obstacles. Straight at the goal when close to it or
         *        when the grid has no path
         */
        Vec3 Direction(const Vec3& from);

        bool HasGoal() const { return m_HasGoal; }
        const Vec3& Goal() const { return m_Goal; }

      private:
        VectorField m_Field;
        Vec3 m_Goal = {0, 0, 0};
        bool m_HasGoal = false;
        bool m_Built = false;
        float m_CellSize = 1.0f;
    };
} // namespace MI
