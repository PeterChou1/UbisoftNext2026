#include "PhysicsGizmos.h"

#include "../ECSManager.h"
#include "../RigidBody.h"
#include "SceneObjects.h"

#include <cmath>

extern ECSManager ECS;

namespace PhysicsGizmos
{
    std::vector<Line> ColliderLines(Entity entity, int circleSegments)
    {
        std::vector<Line> lines;
        std::vector<Vec3> base = SceneObjects::ColliderOutline(entity, circleSegments);
        if (base.empty())
            return lines;
        const Vec3 up(0.0f, SceneObjects::ColliderHeight(entity), 0.0f);
        const std::size_t n = base.size();
        const bool circle = ECS.GetComponent<RigidBody>(entity).Shape.GetShapeType() == CircleShape;
        lines.reserve(n * 2 + (circle ? 4 : n));
        for (std::size_t i = 0; i < n; ++i)
        {
            const Vec3& a = base[i];
            const Vec3& b = base[(i + 1) % n];
            lines.push_back({a, b});
            lines.push_back({a + up, b + up});
            // Every corner of a polygon; four sides of a circle
            if (!circle || i % (n / 4 == 0 ? 1 : n / 4) == 0)
                lines.push_back({a, a + up});
        }
        if (circle)
        {
            // Centre to where the body's angle points: shows it turning
            Vec3 center(0.0f, 0.0f, 0.0f);
            for (const Vec3& p : base)
                center = center + p;
            center = center * (1.0f / static_cast<float>(n));
            RigidBody body = ECS.GetComponent<RigidBody>(entity);
            body.SyncTransform(ECS.GetComponent<Transform>(entity));
            float radius = body.Shape.Radius;
            Vec3 edge = center + Vec3(std::cos(body.Angular) * radius, 0.0f, std::sin(body.Angular) * radius);
            lines.push_back({center + up, edge + up});
        }
        return lines;
    }

    GizmoColor ColorOf(Entity entity, bool simulating)
    {
        if (!ECS.HasComponent<RigidBody>(entity))
            return {};
        const RigidBody& body = ECS.GetComponent<RigidBody>(entity);
        if (simulating && body.IsIntersecting)
            return TOUCHING_COLOR;
        if (!body.Collidable)
            return TRIGGER_COLOR;
        return body.IsStatic() ? STATIC_COLOR : DYNAMIC_COLOR;
    }
} // namespace PhysicsGizmos
