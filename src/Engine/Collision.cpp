#include "Collision.h"

#include "SAT.h"
#include "Utils.h"
#include "stdafx.h"

#include <cassert>
#include <cmath>
#include <limits>

namespace
{
    // A polygon edge (V1 to V2) and its vertex furthest along the normal
    struct Edge
    {
        Vec2 Furthest{};
        Vec2 V1{};
        Vec2 V2{};
        Vec2 Direction{};

        Edge() = default;

        Edge(const Vec2& furthest, const Vec2& v1, const Vec2& v2)
            : Furthest(furthest)
            , V1(v1)
            , V2(v2)
            , Direction(v2 - v1)
        {
        }
    };

    /**
     * \brief The edge next to the polygon's furthest vertex along the normal
     *        that is the most perpendicular to it
     */
    Edge FindClosestEdgeToNormal(const Vec2& normal, const std::vector<Vec2>& poly)
    {
        float maxProjection = -std::numeric_limits<float>::infinity();
        size_t index = 0;
        for (size_t i = 0; i < poly.size(); i++)
        {
            float projection = normal.Dot(poly[i]);
            if (projection > maxProjection)
            {
                maxProjection = projection;
                index = i;
            }
        }

        const Vec2& v = poly[index];
        const Vec2& next = poly[(index + 1) % poly.size()];
        const Vec2& previous = poly[index == 0 ? poly.size() - 1 : index - 1];

        Vec2 left = v - next;
        Vec2 right = v - previous;
        left.Normalize();
        right.Normalize();

        if (right.Dot(normal) <= left.Dot(normal))
            return Edge(v, previous, v);
        return Edge(v, v, next);
    }

    // Up to 3 points, kept on the stack (this runs for every touching pair of
    // polygons, every physics sub step)
    struct ClippedPoints
    {
        Vec2 Points[3];
        int Count = 0;
        void push_back(const Vec2& p) { Points[Count++] = p; }
    };

    /**
     * \brief The part of the segment v1 v2 on the positive side of n . x = o
     */
    ClippedPoints ClipPoints(const Vec2& v1, const Vec2& v2, const Vec2& n, float o)
    {
        ClippedPoints clipped;
        float d1 = n.Dot(v1) - o;
        float d2 = n.Dot(v2) - o;

        if (d1 >= 0.0)
            clipped.push_back(v1);
        if (d2 >= 0.0)
            clipped.push_back(v2);

        if (d1 * d2 < 0.0)
        {
            float u = d1 / (d1 - d2);
            clipped.push_back((v2 - v1) * u + v1);
        }
        return clipped;
    }

    /**
     * \brief Clip the incident edge against the reference edge's side planes,
     *        and keep the points past its face
     */
    void FindContactPoints(const Edge& ref, const Edge& inc, std::vector<Vec2>& contactPoints)
    {
        Vec2 refDirection = ref.Direction;
        refDirection.Normalize();

        float o1 = refDirection.Dot(ref.V1);
        ClippedPoints cp1 = ClipPoints(inc.V1, inc.V2, refDirection, o1);
        if (cp1.Count < 2)
            return;

        float o2 = refDirection.Dot(ref.V2);
        ClippedPoints cp2 = ClipPoints(cp1.Points[0], cp1.Points[1], refDirection * -1.0f, -o2);
        if (cp2.Count < 2)
            return;

        Vec2 refNormal = refDirection.Cross(-1.0f);
        float maxDepth = refNormal.Dot(ref.Furthest);
        for (int i = 0; i < 2; i++)
        {
            if (refNormal.Dot(cp2.Points[i]) - maxDepth >= 0.0)
                contactPoints.push_back(cp2.Points[i]);
        }
    }
} // namespace

void Circle2Circle(Manifold& m, RigidBody& A, RigidBody& B)
{
    const Shape& AShape = A.Shape;
    const Shape& BShape = B.Shape;
    const Vec2& APos = A.Position;
    const Vec2& BPos = B.Position;

    assert(AShape.GetShapeType() == CircleShape && BShape.GetShapeType() == CircleShape);

    float r = AShape.Radius + BShape.Radius;
    float dx = APos.X - BPos.X;
    float dy = APos.Y - BPos.Y;
    m.Collided = dx * dx + dy * dy <= r * r;
    if (!m.Collided || !A.Collidable || !B.Collidable)
        return;

    Vec2 n = APos - BPos;
    float distance = n.GetMagnitude();
    if (distance != 0.0f)
    {
        m.Penetration = r - distance;
        m.Normal = n / distance;
        m.ContactPoints.push_back(BPos + n);
    }
    else
    {
        m.Penetration = AShape.Radius;
        m.Normal = Vec2(1, 0);
        m.ContactPoints.push_back(APos);
    }
}

void Polygon2Polygon(Manifold& m, RigidBody& A, RigidBody& B)
{
    const Shape& AShape = A.Shape;
    const Shape& BShape = B.Shape;
    assert(AShape.GetShapeType() == PolygonShape && BShape.GetShapeType() == PolygonShape);

    const auto& polyA = AShape.PolygonPoints;
    const auto& polyB = BShape.PolygonPoints;
    m.Collided = FindMTVPolygon(
            polyA, polyB, AShape.EdgeNormals, BShape.EdgeNormals, m.Normal, m.Penetration);
    if (!m.Collided || !A.Collidable || !B.Collidable)
        return;

    Edge AEdge = FindClosestEdgeToNormal(m.Normal * -1.0f, polyA);
    Edge BEdge = FindClosestEdgeToNormal(m.Normal, polyB);
    // The edge most perpendicular to the normal is the reference one
    if (std::abs(AEdge.Direction.Dot(m.Normal)) <= std::abs(BEdge.Direction.Dot(m.Normal)))
        FindContactPoints(AEdge, BEdge, m.ContactPoints);
    else
        FindContactPoints(BEdge, AEdge, m.ContactPoints);
}

void Polygon2Circle(Manifold& m, RigidBody& A, RigidBody& B)
{
    const Shape& AShape = A.Shape;
    const Shape& BShape = B.Shape;
    assert(AShape.GetShapeType() == PolygonShape && BShape.GetShapeType() == CircleShape);
    const std::vector<Vec2>& poly = AShape.PolygonPoints;

    m.Collided = FindMTVCircle(
            B.Position, BShape.Radius, poly, AShape.EdgeNormals, m.Normal, m.Penetration);
    if (!m.Collided || !A.Collidable || !B.Collidable)
        return;

    // The contact is the point of the polygon's outline closest to the center
    float minSquaredDistance = std::numeric_limits<float>::infinity();
    Vec2 closest{};
    bool found = false;
    for (size_t i = 0; i < poly.size(); i++)
    {
        Vec2 contactPoint =
                Utils::PointToLineSegment(B.Position, poly[i], poly[(i + 1) % poly.size()]);
        float squaredDistance = (contactPoint - B.Position).GetMagnitudeSquared();
        if (squaredDistance < minSquaredDistance)
        {
            minSquaredDistance = squaredDistance;
            closest = contactPoint;
            found = true;
        }
    }
    if (found)
        m.ContactPoints.push_back(closest);
}

void Circle2Polygon(Manifold& m, RigidBody& A, RigidBody& B)
{
    Polygon2Circle(m, B, A);
    m.Normal *= -1.0f;
}
