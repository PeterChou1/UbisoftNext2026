//---------------------------------------------------------------------------------
// Manifolds.h
//---------------------------------------------------------------------------------
//
// A manifold holds the collision information of two bodies (see Collision.h)
// and resolves their collision
//
#pragma once

#include "RigidBody.h"
#include "Vec2.h"

#include <vector>

class Manifold
{
  public:
    /**
     * \brief Runs the narrow phase test of A and B
     */
    Manifold(Entity entityA, Entity entityB, RigidBody& A, RigidBody& B);

    /**
     * \brief Resolve Collision accounting for rotation
     */
    void ResolveCollisionAngular();

    /**
     * \brief Separates A and B so that they no longer are intersecting
     *        This is an important part of the physics pipeline or else
     *        The bodies will slowly sink at rest
     */
    void PositionCorrection();

    Entity EntityA;
    Entity EntityB;
    RigidBody& A;
    RigidBody& B;
    // whether or not A and B collided
    bool Collided{};
    // how much A has penetrated into B if collided
    float Penetration{};
    // collision normal
    Vec2 Normal{};
    // contact points used for applying impulses to when resolving collision
    std::vector<Vec2> ContactPoints;
};
