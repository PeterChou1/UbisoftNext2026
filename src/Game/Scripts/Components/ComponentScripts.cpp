#include "ComponentScripts.h"

#include "GameComponents.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float ARRIVE_DISTANCE = 0.05f;
}

void WaypointFollower::OnStart()
{
    if (Has<Waypoint>(Self()))
        m_Target = Get<Waypoint>(Self()).Next;
}

void WaypointFollower::OnUpdate(float deltaSeconds)
{
    if (!IsAlive(m_Target))
        return;
    if (m_Wait > 0.0f)
    {
        m_Wait -= deltaSeconds;
        return;
    }
    Vec3 position = Position();
    Vec3 goal = PositionOf(m_Target);
    goal.Y = position.Y;
    Vec3 toGoal = goal - position;
    float distance = std::sqrt(toGoal.X * toGoal.X + toGoal.Z * toGoal.Z);
    float step = std::max(Param("Speed"), 0.0f) * deltaSeconds;
    if (distance > step + ARRIVE_DISTANCE)
    {
        SetPosition(position + toGoal * (step / distance));
        return;
    }
    // Arrived: wait there, then head for the waypoint's Next
    SetPosition(goal);
    if (Has<Waypoint>(m_Target))
    {
        const Waypoint& point = Get<Waypoint>(m_Target);
        m_Wait = point.WaitSeconds;
        m_Target = point.Next;
    }
    else
        m_Target = NULL_ENTITY;
}

void DamageZone::OnCollisionEnter(Entity other)
{
    Hit(other);
}

void DamageZone::Hit(Entity target)
{
    if (!Has<Health>(target))
        return;
    Health& health = Get<Health>(target);
    if (health.Invulnerable)
        return;
    health.Current = std::max(0.0f, health.Current - Param("Damage"));
    if (health.Current <= 0.0f && health.DestroyAtZero)
        Destroy(target);
}
