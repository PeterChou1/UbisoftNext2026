//---------------------------------------------------------------------------------
// Shape.h
//---------------------------------------------------------------------------------
//
// The collider shape of a rigid body: a circle or a convex polygon
//
#pragma once

#include "Mat2.h"
#include "Utils.h"
#include "Vec2.h"

#include <algorithm>
#include <vector>

enum ShapeType
{
    CircleShape = 0,
    PolygonShape = 1
};

class Shape
{
  public:
    ShapeType GetShapeType() const { return m_ShapeEnum; }

    /**
     * \brief World space points and edge normals from the local points
     */
    void RecomputePoints(const Mat2& rotation, Vec2 position)
    {
        // In place: this runs for every body every physics sub step
        Utils::TranslatePointsInto(LocalSpacePoints, rotation, position, PolygonPoints);
        ComputeEdgeNormals();
    }

    static Shape CreateCircle(float radius)
    {
        Shape collider;
        collider.Radius = radius;
        collider.m_ShapeEnum = CircleShape;
        return collider;
    }

    /**
     * \brief Convex polygon collider. Points are local, around the body origin,
     *        counter clockwise in (x, y) (reordered if given the other way)
     */
    static Shape CreatePolygon(std::vector<Vec2> points)
    {
        float signedArea = 0.0f;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const Vec2& a = points[i];
            const Vec2& b = points[(i + 1) % points.size()];
            signedArea += a.X * b.Y - b.X * a.Y;
        }
        if (signedArea < 0.0f)
            std::reverse(points.begin(), points.end());

        Shape collider;
        collider.LocalSpacePoints = points;
        collider.PolygonPoints = points;
        collider.m_ShapeEnum = PolygonShape;
        Vec2 min = points[0];
        Vec2 max = points[0];
        for (const Vec2& p : points)
        {
            min = Vec2(std::min(min.X, p.X), std::min(min.Y, p.Y));
            max = Vec2(std::max(max.X, p.X), std::max(max.Y, p.Y));
        }
        collider.Width = max.X - min.X;
        collider.Height = max.Y - min.Y;
        return collider;
    }

    static Shape CreateRect(float width, float height)
    {
        Shape collider;
        collider.LocalSpacePoints = {Vec2(-width / 2.0f, -height / 2.0f),
                                     Vec2(width / 2.0f, -height / 2.0f),
                                     Vec2(width / 2.0f, height / 2.0f),
                                     Vec2(-width / 2.0f, height / 2.0f)};
        collider.PolygonPoints = collider.LocalSpacePoints;
        collider.m_ShapeEnum = PolygonShape;
        collider.Width = width;
        collider.Height = height;
        return collider;
    }

    float Width{};
    float Height{};
    float Radius{};
    std::vector<Vec2> PolygonPoints;
    std::vector<Vec2> EdgeNormals;
    std::vector<Vec2> LocalSpacePoints;
    // Saved (or cleared on load) with the shape but not used by the physics
    std::vector<Vec2> DebugPoints;
    std::vector<Vec2> ContactPoints;
    Vec2 Max{};
    Vec2 Min{};

  private:
    // Grants the save system access to private state (see EngineSerialization.h)
    friend struct SerializationAccess;

    void ComputeEdgeNormals()
    {
        const size_t count = PolygonPoints.size();
        EdgeNormals.resize(count);
        for (size_t i = 0; i < count; i++)
        {
            Vec2 edge = PolygonPoints[i] - PolygonPoints[(i + 1) % count];
            EdgeNormals[i] = edge.Cross(-1.0f).Normalize();
        }
    }

    ShapeType m_ShapeEnum{};
};