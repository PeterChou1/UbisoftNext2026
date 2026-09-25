#include "SAT.h"

#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace
{
    // Interval (min, max) of a shape projected on an axis
    using Projection = std::pair<float, float>;

    Projection ProjectPolygon(const std::vector<Vec2>& polygon, const Vec2& axis)
    {
        float min = axis.Dot(polygon[0]);
        float max = min;
        for (const Vec2& p : polygon)
        {
            float projection = axis.Dot(p);
            min = std::min(min, projection);
            max = std::max(max, projection);
        }
        return {min, max};
    }

    Projection ProjectCircle(const Vec2& center, float radius, const Vec2& axis)
    {
        float p1 = (center + axis * radius).Dot(axis);
        float p2 = (center - axis * radius).Dot(axis);
        return {std::min(p1, p2), std::max(p1, p2)};
    }

    /**
     * \brief Test one separating axis: false when the projections are apart,
     *        otherwise keeps the axis as the collision normal (pointing from
     *        b towards a) when its overlap is the smallest so far
     */
    bool TestAxis(const Vec2& axis,
                  const Projection& a,
                  const Projection& b,
                  Vec2& collisionNormal,
                  float& overlapAmount)
    {
        if (!(a.first <= b.second && a.second >= b.first))
            return false;

        float overlap = std::min(a.second, b.second) - std::max(a.first, b.first);
        bool flip = !(a.second > b.second);

        // One interval contains the other: add the distance to the nearest end
        if ((a.first <= b.first && b.second <= a.second) ||
            (b.first <= a.first && a.second <= b.second))
        {
            float mins = std::abs(a.first - b.first);
            float maxs = std::abs(a.second - b.second);
            if (mins < maxs)
            {
                overlap += mins;
                flip = true;
            }
            else
            {
                overlap += maxs;
                flip = false;
            }
        }

        if (overlap < overlapAmount)
        {
            overlapAmount = overlap;
            collisionNormal = flip ? axis * -1 : axis;
        }
        return true;
    }
} // namespace

bool FindMTVCircle(const Vec2& center,
                   float radius,
                   const std::vector<Vec2>& poly,
                   const std::vector<Vec2>& normals,
                   Vec2& collisionNormal,
                   float& overlapAmount)
{
    overlapAmount = std::numeric_limits<float>::max();
    for (const Vec2& axis : normals)
    {
        if (!TestAxis(axis,
                      ProjectPolygon(poly, axis),
                      ProjectCircle(center, radius, axis),
                      collisionNormal,
                      overlapAmount))
            return false;
    }
    return true;
}

bool FindMTVPolygon(const std::vector<Vec2>& poly1,
                    const std::vector<Vec2>& poly2,
                    const std::vector<Vec2>& normals1,
                    const std::vector<Vec2>& normals2,
                    Vec2& collisionNormal,
                    float& overlapAmount)
{
    overlapAmount = std::numeric_limits<float>::max();
    // The edge normals of both polygons
    for (const std::vector<Vec2>* normals : {&normals1, &normals2})
    {
        for (const Vec2& axis : *normals)
        {
            if (!TestAxis(axis,
                          ProjectPolygon(poly1, axis),
                          ProjectPolygon(poly2, axis),
                          collisionNormal,
                          overlapAmount))
                return false;
        }
    }
    return true;
}
