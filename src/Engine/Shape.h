//---------------------------------------------------------------------------------
// Shape.h
//---------------------------------------------------------------------------------
//
// Class used to represent a shape within rigid body stores
// all shape information in the class depending on what shape it is
//
#pragma once

#include <algorithm>

#include "Utils.h"
#include "Vec2.h"

enum ShapeType
{
    CircleShape = 0,
    PolygonShape = 1
};

class Shape
{
  public:
    Shape() = default;

    ShapeType GetShapeType() const { return m_ShapeEnum; }

    void RecomputePoints(float angle, Vec2 position)
    {
        // In place: this runs for every body every physics sub step
        Utils::TranslatePointsInto(LocalSpacePoints, angle, position, PolygonPoints);
        ComputeEdgeNormals();
    }

    /**
     * \brief RecomputePoints with the rotation matrix already built
     */
    void RecomputePoints(const Mat2& rotation, Vec2 position)
    {
        Utils::TranslatePointsInto(LocalSpacePoints, rotation, position, PolygonPoints);
        ComputeEdgeNormals();
    }

    void ComputeEdgeNormals()
    {
        size_t polySize = PolygonPoints.size();
        EdgeNormals.resize(polySize);
        for (size_t i = 0; i < PolygonPoints.size(); i++)
        {
            Vec2 edge = PolygonPoints[i] - PolygonPoints[(i + 1) % polySize];
            EdgeNormals[i] = edge.Cross(-1.0).Normalize();
        }
    }

    static Shape CreateCircle(float radius)
    {
        auto collider = Shape();
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

        auto collider = Shape();
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
        auto collider = Shape();
        collider.LocalSpacePoints.push_back(Vec2(-width / 2.0f, -height / 2.0f));
        collider.LocalSpacePoints.push_back(Vec2(width / 2.0f, -height / 2.0f));
        collider.LocalSpacePoints.push_back(Vec2(width / 2.0f, height / 2.0f));
        collider.LocalSpacePoints.push_back(Vec2(-width / 2.0f, height / 2.0f));
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
    std::vector<Vec2> DebugPoints;
    std::vector<Vec2> ContactPoints;
    Vec2 Max{};
    Vec2 Min{};

  private:
    // Grants the save system access to private state (see EngineSerialization.h)
    friend struct SerializationAccess;

    ShapeType m_ShapeEnum{};
};