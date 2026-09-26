#include "Manifolds.h"

#include "Collision.h"
#include "stdafx.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace
{
    using CollisionTest = void (*)(Manifold& m, RigidBody& A, RigidBody& B);

    // Indexed by the ShapeType of A, then of B
    const CollisionTest COLLISION_TESTS[2][2] = {{Circle2Circle, Circle2Polygon},
                                                 {Polygon2Circle, Polygon2Polygon}};
} // namespace

Manifold::Manifold(Entity entityA, Entity entityB, RigidBody& A, RigidBody& B)
    : EntityA(entityA)
    , EntityB(entityB)
    , A(A)
    , B(B)
{
    COLLISION_TESTS[A.Shape.GetShapeType()][B.Shape.GetShapeType()](*this, A, B);
}

void Manifold::ResolveCollisionAngular()
{
    if (!A.Collidable || !B.Collidable)
        return;

    assert(std::abs(Normal.GetMagnitude() - 1.0) < 0.01);

    float e = std::min(A.Restitution(), B.Restitution());
    float staticFriction = (A.StaticFriction + B.StaticFriction) / 2.0f;
    float dynamicFriction = (A.DynamicFriction + B.DynamicFriction) / 2.0f;
    const int count = static_cast<int>(ContactPoints.size());

    // At most 2 contact points (a clipped polygon edge, or 1 for circles):
    // fixed arrays instead of vectors allocated per contact per sub step
    constexpr int MAX_CONTACTS = 4;
    assert(count <= MAX_CONTACTS);
    float impulses[MAX_CONTACTS];
    // Contact points relative to A and B
    Vec2 contactA[MAX_CONTACTS];
    Vec2 contactB[MAX_CONTACTS];

    // Velocity of the contact point on B relative to the one on A
    auto relativeVelocity = [&](const Vec2& raPerp, const Vec2& rbPerp) {
        Vec2 combinedVelocityA = A.Velocity + raPerp * A.AngularVelocity;
        Vec2 combinedVelocityB = B.Velocity + rbPerp * B.AngularVelocity;
        return combinedVelocityB - combinedVelocityA;
    };
    // Mass along a direction at the contact
    auto inverseMass = [&](float normalA, float normalB) {
        return A.InvMass() + B.InvMass() + (normalA * normalA) * A.InvInertia() +
               (normalB * normalB) * B.InvInertia();
    };

    for (int i = 0; i < count; i++)
    {
        Vec2 ra = ContactPoints[i] - A.Position;
        Vec2 rb = ContactPoints[i] - B.Position;
        Vec2 raPerp = ra.Cross(-1.0f);
        Vec2 rbPerp = rb.Cross(-1.0f);

        float vNormal = relativeVelocity(raPerp, rbPerp).Dot(Normal);
        // Already separating
        if (vNormal < 0.0f)
            return;

        float j = -(1.0f + e) * vNormal;
        j /= inverseMass(raPerp.Dot(Normal), rbPerp.Dot(Normal));
        j /= static_cast<float>(count);

        impulses[i] = j;
        contactA[i] = ra;
        contactB[i] = rb;
    }

    for (int i = 0; i < count; i++)
    {
        Vec2 impulse = Normal * impulses[i];
        A.ApplyImpulseAngular(impulse * -1.0f, contactA[i]);
        B.ApplyImpulseAngular(impulse, contactB[i]);
    }

    // Friction impulse of each contact (none where it does not slide), all
    // computed from the velocities after the normal impulses
    Vec2 frictionImpulses[MAX_CONTACTS];
    bool hasFriction[MAX_CONTACTS] = {};

    for (int i = 0; i < count; i++)
    {
        Vec2 raPerp = contactA[i].Cross(-1.0f);
        Vec2 rbPerp = contactB[i].Cross(-1.0f);
        Vec2 velocity = relativeVelocity(raPerp, rbPerp);

        Vec2 tangent = velocity - Normal * velocity.Dot(Normal);
        if (std::abs(tangent.X) <= std::numeric_limits<float>::epsilon() &&
            std::abs(tangent.Y) <= std::numeric_limits<float>::epsilon())
            continue;
        tangent = tangent.Normalize();

        float jt = -1.0f * velocity.Dot(tangent);
        jt /= inverseMass(raPerp.Dot(tangent), rbPerp.Dot(tangent));
        jt /= static_cast<float>(count);

        // Coulomb's law
        if (std::abs(jt) <= impulses[i] * staticFriction)
            frictionImpulses[i] = tangent * jt;
        else
            frictionImpulses[i] = tangent * -1.0f * impulses[i] * dynamicFriction;
        hasFriction[i] = true;
    }

    for (int i = 0; i < count; i++)
    {
        if (!hasFriction[i])
            continue;
        A.ApplyImpulseAngular(frictionImpulses[i], contactA[i]);
        B.ApplyImpulseAngular(frictionImpulses[i] * -1.0f, contactB[i]);
    }
}

void Manifold::PositionCorrection()
{
    if (!A.Collidable || !B.Collidable)
        return;

    constexpr float PERCENT = 0.8f;
    // Penetration allowed without correction (avoids jitter at rest)
    constexpr float SLOP = 0.01f;
    Vec2 correction =
            Normal * std::max(Penetration - SLOP, 0.0f) / (A.InvMass() + B.InvMass()) * PERCENT;
    A.Position += correction * A.InvMass();
    B.Position -= correction * B.InvMass();
}
