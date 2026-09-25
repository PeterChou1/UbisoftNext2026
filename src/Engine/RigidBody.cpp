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

void RigidBody::SetStatic()
{
    m_InvMass = 0.0f;
    m_InvInertia = 0.0f;
}

void RigidBody::SyncTransform(Transform& transform)
{
    // Bodies live in world space: a child's body follows its parents
    Vec3 position = transform.GetWorldPosition();
    Quat rotation = transform.GetWorldRotation();
    switch (transform.Plane)
    {
    case YZ: {
        Position.X = position.Y;
        Position.Y = position.Z;
        Angular = rotation.GetRoll2D();
        break;
    }
    case XZ: {
        Position.X = position.X;
        Position.Y = position.Z;
        Angular = -rotation.GetPitch2D();
        break;
    }
    case XY: {
        Position.X = position.X;
        Position.Y = position.Y;
        Angular = rotation.GetYaw2D();
        break;
    }
    }
}

void RigidBody::ForwardTransform(Transform& transform) const
{
    if (!Collidable)
        return;

    // transform.SetPosition2D(Position);
    // World position (the same as the local one for a root transform)
    Vec3 current = transform.GetWorldPosition();
    switch (transform.Plane)
    {
    case YZ: {
        Vec3 Loc = Vec3(current.X, Position.X, Position.Y);
        transform.SetWorldPosition(Loc);
        transform.UpdateLocalRow(AngularDelta);
        break;
    }
    case XZ: {
        Vec3 Loc = Vec3(Position.X, current.Y, Position.Y);
        transform.SetWorldPosition(Loc);
        // SyncTransform reads Angular = -pitch on this plane: turning the
        // transform by +delta made a spinning body turn the other way
        transform.UpdateLocalPitch(TransformTurn(transform.Plane, AngularDelta));
        break;
    }
    case XY: {
        Vec3 Loc = Vec3(Position.X, Position.Y, current.Z);
        transform.SetWorldPosition(Loc);
        transform.UpdateLocalYaw(AngularDelta);
    }
    }
}

void RigidBody::ForwardPosition(Transform& transform) const
{
    Vec3 current = transform.GetWorldPosition();
    switch (transform.Plane)
    {
    case YZ:
        transform.SetWorldPosition(Vec3(current.X, Position.X, Position.Y));
        break;
    case XZ:
        transform.SetWorldPosition(Vec3(Position.X, current.Y, Position.Y));
        break;
    case XY:
        transform.SetWorldPosition(Vec3(Position.X, Position.Y, current.Z));
        break;
    }
}

float RigidBody::TransformTurn(SlicePlane plane, float angularDelta)
{
    return plane == XZ ? -angularDelta : angularDelta;
}

Vec3 RigidBody::RotationAxis(SlicePlane plane)
{
    // The axes of Transform::UpdateLocalRow / Pitch / Yaw
    switch (plane)
    {
    case YZ:
        return Vec3(1, 0, 0);
    case XZ:
        return Vec3(0, 1, 0);
    default:
        return Vec3(0.0f, 0.0f, 1.0);
    }
}

void RigidBody::RecomputeGeometry()
{
    if (Shape.GetShapeType() == CircleShape)
    {
        Shape.RecomputePoints(Mat2(), Position);
        RigidBodyAABB.RecomputeAABB(Position, Mat2(), CircleShape);
        return;
    }
    Mat2 rotation = Utils::RotationMatrix(Angular);
    Shape.RecomputePoints(rotation, Position);
    RigidBodyAABB.RecomputeAABB(Position, rotation, PolygonShape);
}

void RigidBody::ApplyImpulseAngular(const Vec2& impulse, const Vec2& contactVector)
{
    Velocity += impulse * m_InvMass;
    AngularVelocity += contactVector.Cross(impulse) * m_InvInertia;
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
