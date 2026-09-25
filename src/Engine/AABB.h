//---------------------------------------------------------------------------------
// AABB.h
//---------------------------------------------------------------------------------
//
// Axis Align Bounding Box Used for cheap broad phase intersection test
// before the narrow phase
//

#pragma once
#include "Mat2.h"
#include "Shape.h"
#include "Utils.h"
#include "Vec2.h"

#include <cassert>
#include <cmath>
#include <vector>

struct AABB
{
    Vec2 OriginalMax{};
    Vec2 OriginalMin{};

    Vec2 Max{};
    Vec2 Min{};

    AABB() = default;

    /**
     * \brief Computes AABB for Circle
     * \param radius of circle
     */
    AABB(float radius)
    {
        OriginalMax = Vec2(radius, radius);
        OriginalMin = Vec2(-radius, -radius);
        Min = OriginalMin;
        Max = OriginalMax;
    }

    /**
     * \brief Compute AABB for a polygon
     * \param points points in the polygon
     */
    AABB(const std::vector<Vec2>& points)
    {
        assert(!points.empty() && "Points Empty");
        ComputeMaxPoints(points, OriginalMax, OriginalMin);
        Min = OriginalMin;
        Max = OriginalMax;
    }

    /**
     * \brief Recomputes AABB at the end of physics pipeline
     */
    void RecomputeAABB(const Vec2& newPosition, float newAngle, ShapeType shapeType)
    {
        if (shapeType == CircleShape)
            RecomputeAABB(newPosition, Mat2(), shapeType);
        else
            RecomputeAABB(newPosition, Mat2(Vec2(std::cos(newAngle), -std::sin(newAngle)),
                                            Vec2(std::sin(newAngle), std::cos(newAngle))),
                          shapeType);
    }

    /**
     * \brief The same with the rotation matrix already built (unused for
     *        circles)
     */
    void RecomputeAABB(const Vec2& newPosition, Mat2 matrix, ShapeType shapeType)
    {
        // On the stack (every body, every physics sub step), the same
        // arithmetic as Utils::TranslatePoints
        Vec2 points[4] = {OriginalMax,
                          Vec2(OriginalMin.X, OriginalMax.Y),
                          OriginalMin,
                          Vec2(OriginalMax.X, OriginalMin.Y)};
        if (shapeType == CircleShape)
        {
            for (Vec2& point : points)
                point += newPosition;
        }
        else
        {
            for (Vec2& point : points)
            {
                Vec2 local = point;
                point = matrix * local + newPosition;
            }
        }
        Max = points[0];
        Min = points[0];
        for (const Vec2& point : points)
        {
            if (point.X < Min.X)
                Min.X = point.X;
            if (point.Y < Min.Y)
                Min.Y = point.Y;
            if (point.X > Max.X)
                Max.X = point.X;
            if (point.Y > Max.Y)
                Max.Y = point.Y;
        }
    }

    static void ComputeMaxPoints(const std::vector<Vec2>& points, Vec2& maxPoint, Vec2& minPoint)
    {
        maxPoint = points[0];
        minPoint = points[0];
        for (const Vec2& point : points)
        {
            if (point.X < minPoint.X)
                minPoint.X = point.X;
            if (point.Y < minPoint.Y)
                minPoint.Y = point.Y;
            if (point.X > maxPoint.X)
                maxPoint.X = point.X;
            if (point.Y > maxPoint.Y)
                maxPoint.Y = point.Y;
        }
    }
};

/**
 * \brief AABB intersection test returns true depending if A and B
 *        are colliding
 */
inline bool AABBTest(AABB& A, AABB& B)
{
    if (A.Max.X < B.Min.X || A.Min.X > B.Max.X)
        return false;
    if (A.Max.Y < B.Min.Y || A.Min.Y > B.Max.Y)
        return false;
    return true;
}

/**
 * \brief AABB intersection test with a point
 */
inline bool AABBPoint(AABB& A, float x, float y)
{
    if (A.Min.X > x || x > A.Max.X)
        return false;
    if (A.Min.Y > y || y > A.Max.Y)
        return false;
    return true;
}