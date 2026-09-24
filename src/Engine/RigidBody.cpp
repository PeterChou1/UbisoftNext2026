#include "RigidBody.h"

#include "app.h"
#include "stdafx.h"

#include <cassert>

RigidBody::RigidBody(float radius)
{
    Shape = Shape::CreateCircle(radius);
    RigidBodyAABB = AABB(radius);
    float area = radius * radius * 3.141f;
    float mass = DEFAULT_DENSITY * area;
    float inertia = 0.5f * mass * radius * radius;
    m_InvMass = 1.0f / mass;
    m_InvInertia = 1.0f / inertia;
    m_Restitution = DEFAULT_RESTITUTION;
    StaticFriction = DEFAULT_STATIC_FRICTION;
    DynamicFriction = DEFAULT_DYNAMIC_FRICTION;
}

RigidBody::RigidBody(float width, float height, float weightMultiplier)
{
    Shape = Shape::CreateRect(width, height);
    RigidBodyAABB = AABB(Shape.PolygonPoints);
    float area = width * height;
    float mass = area * DEFAULT_DENSITY * weightMultiplier;
    float inertia = (1.0f / 12.0f) * mass * (width * width + height * height);
    m_InvMass = 1.0f / mass;
    m_InvInertia = 1.0f / inertia;
    m_Restitution = DEFAULT_RESTITUTION;
    StaticFriction = DEFAULT_STATIC_FRICTION;
    DynamicFriction = DEFAULT_DYNAMIC_FRICTION;
}

RigidBody::RigidBody(std::vector<Vec2> polygons)
{
    assert(polygons.size() >= 3 && "A polygon needs at least 3 points");
    Shape = Shape::CreatePolygon(std::move(polygons));
    const std::vector<Vec2>& p = Shape.LocalSpacePoints;
    RigidBodyAABB = AABB(p);

    // Area and moment of inertia (about the body origin) of a polygon, summed
    // over the triangles (origin, p[i], p[i + 1])
    float area = 0.0f;
    float inertiaSum = 0.0f;
    for (size_t i = 0; i < p.size(); ++i)
    {
        const Vec2& a = p[i];
        const Vec2& b = p[(i + 1) % p.size()];
        float cross = a.X * b.Y - b.X * a.Y;
        area += cross * 0.5f;
        inertiaSum += cross * (a.X * a.X + a.X * b.X + b.X * b.X + a.Y * a.Y + a.Y * b.Y +
                               b.Y * b.Y);
    }
    float mass = DEFAULT_DENSITY * area;
    float inertia = DEFAULT_DENSITY * inertiaSum / 12.0f;
    m_InvMass = 1.0f / mass;
    m_InvInertia = 1.0f / inertia;
    m_Restitution = DEFAULT_RESTITUTION;
    StaticFriction = DEFAULT_STATIC_FRICTION;
    DynamicFriction = DEFAULT_DYNAMIC_FRICTION;
}

void RigidBody::UpdateRadius(float radius)
{
    Shape = Shape::CreateCircle(radius);
    RigidBodyAABB = AABB(radius);
    float area = radius * radius * 3.141f;
    float mass = DEFAULT_DENSITY * area;
    float inertia = 0.5f * mass * radius * radius;
    m_InvMass = 1.0f / mass;
    m_InvInertia = 1.0f / inertia;
}

void RigidBody::SetStatic()
{
    m_InvMass = 0.0f;
    m_InvInertia = 0.0f;
}

void RigidBody::SyncTransform(Transform& transform)
{
    switch (transform.Plane)
    {
    case YZ: {
        Position.X = transform.LocalPosition.Y;
        Position.Y = transform.LocalPosition.Z;
        Angular = transform.LocalRotation.GetRoll2D();
        break;
    }
    case XZ: {
        Position.X = transform.LocalPosition.X;
        Position.Y = transform.LocalPosition.Z;
        Angular = -transform.LocalRotation.GetPitch2D();
        break;
    }
    case XY: {
        Position.X = transform.LocalPosition.X;
        Position.Y = transform.LocalPosition.Y;
        Angular = transform.LocalRotation.GetYaw2D();
        break;
    }
    }
}

void RigidBody::ForwardTransform(Transform& transform) const
{
    if (!Collidable)
        return;

    // transform.SetPosition2D(Position);
    switch (transform.Plane)
    {
    case YZ: {
        Vec3 Loc = Vec3(transform.LocalPosition.X, Position.X, Position.Y);
        transform.SetLocalPosition(Loc);
        transform.UpdateLocalRow(AngularDelta);
        break;
    }
    case XZ: {
        Vec3 Loc = Vec3(Position.X, transform.LocalPosition.Y, Position.Y);
        transform.SetLocalPosition(Loc);
        transform.UpdateLocalPitch(AngularDelta);
        break;
    }
    case XY: {
        Vec3 Loc = Vec3(Position.X, Position.Y, transform.LocalPosition.Z);
        transform.SetLocalPosition(Loc);
        transform.UpdateLocalYaw(AngularDelta);
    }
    }
}

void RigidBody::RecomputeAABB()
{
    RigidBodyAABB.RecomputeAABB(Position, Angular, Shape.GetShapeType());
}

void RigidBody::ApplyImpulse(const Vec2& impulse)
{
    Velocity += impulse * m_InvMass;
}

void RigidBody::ApplyImpulseAngular(const Vec2& impulse, const Vec2& contactVector)
{
    Velocity += impulse * m_InvMass;
    AngularVelocity += contactVector.Cross(impulse) * m_InvInertia;
}

void RigidBody::IntegrateVelocity(float deltaTime)
{
    if (m_InvMass == 0.0f)
        return;
    Position += Velocity * deltaTime;
}

void RigidBody::IntegrateVelocityAngular(float deltaTime)
{
    if (m_InvMass == 0.0f || !Collidable)
        return;
    Position += Velocity * deltaTime;
    AngularDelta = AngularVelocity * deltaTime;
    Angular += AngularDelta;
}

float RigidBody::InvMass() const
{
    return m_InvMass;
}

float RigidBody::InvInertia() const
{
    return m_InvInertia;
}

float RigidBody::Restitution() const
{
    return m_Restitution;
}
