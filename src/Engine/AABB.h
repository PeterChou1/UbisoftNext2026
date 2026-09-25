//---------------------------------------------------------------------------------
// AABB.h
//---------------------------------------------------------------------------------
//
// Axis aligned bounding box, used for a cheap broad phase intersection test
// before the narrow phase
//
#pragma once

#include "Mat2.h"
#include "Shape.h"
#include "Vec2.h"

#include <cassert>
#include <cstddef>
#include <vector>

struct AABB
{
    // The box around the body origin, and around its current position
    Vec2 OriginalMax{};
    Vec2 OriginalMin{};
    Vec2 Max{};
    Vec2 Min{};

    AABB() = default;

    /**
     * \brief Box of a circle
     */
    AABB(float radius)
        : OriginalMax(radius, radius)
        , OriginalMin(-radius, -radius)
        , Max(OriginalMax)
        , Min(OriginalMin)
    {
    }

    /**
     * \brief Box of a polygon
     */
    AABB(const std::vector<Vec2>& points)
    {
        assert(!points.empty() && "Points Empty");
        Bounds(points.data(), points.size(), OriginalMax, OriginalMin);
        Min = OriginalMin;
        Max = OriginalMax;
    }

    /**
     * \brief Recomputes the box from the body's position and rotation
     *        matrix (unused for circles)
     */
    void RecomputeAABB(const Vec2& newPosition, Mat2 matrix, ShapeType shapeType)
    {
        // On the stack (every body, every physics sub step), the same
        // arithmetic as Utils::TranslatePointsInto
        Vec2 points[4] = {OriginalMax,
                          Vec2(OriginalMin.X, OriginalMax.Y),
                          OriginalMin,
                          Vec2(OriginalMax.X, OriginalMin.Y)};
        for (Vec2& point : points)
            point = shapeType == CircleShape ? point + newPosition : matrix * point + newPosition;
        Bounds(points, 4, Max, Min);
    }

  private:
    static void Bounds(const Vec2* points, std::size_t count, Vec2& max, Vec2& min)
    {
        max = points[0];
        min = points[0];
        for (std::size_t i = 0; i < count; ++i)
        {
            const Vec2& point = points[i];
            if (point.X < min.X)
                min.X = point.X;
            if (point.Y < min.Y)
                min.Y = point.Y;
            if (point.X > max.X)
                max.X = point.X;
            if (point.Y > max.Y)
                max.Y = point.Y;
        }
    }
};

/**
 * \brief Whether the boxes A and B overlap
 */
inline bool AABBTest(const AABB& A, const AABB& B)
{
    return !(A.Max.X < B.Min.X || A.Min.X > B.Max.X || A.Max.Y < B.Min.Y || A.Min.Y > B.Max.Y);
}
