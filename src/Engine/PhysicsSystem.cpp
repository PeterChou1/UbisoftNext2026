#include "PhysicsSystem.h"

#include "ECSManager.h"
#include "Manifolds.h"
#include "RigidBody.h"
#include "Transform.h"
#include "stdafx.h"

#include <iostream>

extern ECSManager ECS;

// Step time of each physics simulation decrease for more accuracy
// at the cost of performance
constexpr float StepTime = 16.0f;
constexpr float dt = StepTime / 1000.0f;
// Maximum time a physics step will process this is to prevent
// a massive lag spike from overwhelming the physics logic
constexpr float MaxTime = 50.0f;
// How strong you want the gravity to be
constexpr float GravityScale = 5.0f;
Vec2 Gravity = Vec2(0.0f, -10.0f * GravityScale);

PhysicsSystem::PhysicsSystem()
{
    m_CallbackSystem = ECS.GetResource<ColliderCallbackSystem>();
}

void PhysicsSystem::SyncData()
{
    // Sync Transform -> RigidBody
    // Sync ECS -> m_RigidBodies
    m_RigidBodies.clear();
    for (auto& e : ECS.Visit<RigidBody, Transform>())
    {
        RigidBody& rigidbody = ECS.GetComponent<RigidBody>(e);
        rigidbody.Color = Vec3(1.0f, 0.0f, 0.0f);
        m_RigidBodies.emplace_back(e, rigidbody);
        Transform& transform = ECS.GetComponent<Transform>(e);
        rigidbody.SyncTransform(transform);
        rigidbody.RecomputeAABB();
        rigidbody.Shape.RecomputePoints(rigidbody.Angular, rigidbody.Position);
        if (!rigidbody.Initialized)
            rigidbody.Initialized = true;
    }
}

void PhysicsSystem::ForwardTransform()
{
    for (auto& e : ECS.Visit<RigidBody, Transform>())
    {
        RigidBody& rigidbody = ECS.GetComponent<RigidBody>(e);
        // don't bother updating masses with infinite mass
        Transform& transform = ECS.GetComponent<Transform>(e);
        rigidbody.ForwardTransform(transform);
    }
}

void PhysicsSystem::Update(float deltaTime)
{
    m_Accumulate += deltaTime;

    if (m_Accumulate > MaxTime)
        m_Accumulate = MaxTime;

    // Physics is updated once every 16ms
    while (m_Accumulate > StepTime)
    {
        SyncData();
        for (int i = 0; i < STEP_ITERATION; i++)
        {
            Step();
            ForwardTransform();
            m_CallbackSystem->Update();
        }
        // One fixed step consumed (was MaxTime, which ran the simulation at
        // roughly a third of real time)
        m_Accumulate -= StepTime;
    }
}

void PhysicsSystem::Step()
{
    m_Collisions.clear();
    float deltaTime = dt / STEP_ITERATION;

    // Reset is intersecting
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rb = collisionPair.second;
        rb.IsIntersecting = false;
    }

    // Broad Phase Collision
    std::vector<Manifold> collisions;
    for (auto& collisionPair1 : m_RigidBodies)
    {
        for (auto& collisionPair2 : m_RigidBodies)
        {
            Entity e1 = collisionPair1.first;
            Entity e2 = collisionPair2.first;
            if (e1 == e2 || e1 > e2)
                continue;

            RigidBody& r1 = collisionPair1.second;
            RigidBody& r2 = collisionPair2.second;

            // don't bother resolving collision between two infinite mass
            if (r1.InvMass() == 0.0 && r2.InvMass() == 0.0f)
                continue;

            if (AABBTest(r1.RigidBodyAABB, r2.RigidBodyAABB))
            {
                // Narrow phase
                // NOTE: Right now narrow phase detection is the bottleneck
                //       implementing spatial acceleration structures like
                //       quad trees probably won't speed up physics step computation
                Manifold m = Manifold(e1, e2, r1, r2);
                if (m.Collided)
                {
                    r1.IsIntersecting = true;
                    r2.IsIntersecting = true;
                    m_Collisions.push_back(m);
                    m_CallbackSystem->SubmitContact(e1, e2);
                    if (m_CallbackSystem->HasRegisterCallback({r1.Category, r2.Category}))
                    {
                        m_CallbackSystem->SubmitForCallback(e1, e2);
                    }
                }
            }
        }
    }

    // Integrate Forces
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rigidbody = collisionPair.second;
        if (rigidbody.InvMass() == 0.0 || !rigidbody.Collidable)
            continue;

        // Apply Gravity
        // rigidbody.Velocity += Gravity * deltaTime;
        // Calculate normal force (assuming flat surface and gravity only)

        Vec2 normalForce(0.0f, Gravity.Y);
        // Calculate friction force
        Vec2 frictionForce;
        if (rigidbody.Velocity.GetMagnitudeSquared() > 0)
        {
            // Kinetic friction
            Vec2 rbVelocity = rigidbody.Velocity * -1.0f;
            rbVelocity.Normalize();
            frictionForce = rbVelocity * normalForce.GetMagnitude() * rigidbody.DynamicFriction;
        }
        else
        {
            frictionForce = Vec2(0.0f, 0.0f);
        }
        rigidbody.Velocity += frictionForce * deltaTime;
    }

    // Resolve Collision
    for (auto& manifold : m_Collisions)
    {
        manifold.ResolveCollisionAngular();
    }

    // Integrate Velocity
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rigidbody = collisionPair.second;
        rigidbody.IntegrateVelocityAngular(deltaTime);
    }

    // Correct Position
    for (auto& manifold : m_Collisions)
    {
        manifold.PositionCorrection();
    }

    // Clear All Forces
    // Sync Rigid Body -> Transform
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rigidbody = collisionPair.second;
        rigidbody.Force = Vec2(0.0f, 0.0f);
        rigidbody.Shape.RecomputePoints(rigidbody.Angular, rigidbody.Position);
        rigidbody.RecomputeAABB();
    }
}
