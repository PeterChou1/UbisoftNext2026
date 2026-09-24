//---------------------------------------------------------------------------------
// GameplayScripts.h
//---------------------------------------------------------------------------------
//
// Object scripts implementing simple game rules through contact events.
// Contacts come from the physics system, so the object needs a body: use a
// Trigger body for things the player passes through (pickups, hazards).
//
#pragma once

#include "MovementScripts.h"
#include "Scripting/Script.h"

/**
 * \brief Collected when the Player touches it: adds Points to the score of the
 *        CollectGame scene script and disappears. Spins at SpinSpeed
 */
class Collectible : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;
    void OnCollisionEnter(Entity other) override;
};

/**
 * \brief Touching it costs the Player a life (see CollectGame)
 */
class Hazard : public Script
{
  public:
    void OnCollisionEnter(Entity other) override;
};

/**
 * \brief A Patrol (moves back and forth) that hurts the Player it touches
 */
class MovingHazard : public Patrol
{
  public:
    void OnCollisionEnter(Entity other) override;
};

/**
 * \brief A Mover (flies forward, expires) that hurts the Player it touches.
 *        Speed: units per second, Lifetime: seconds
 */
class Projectile : public Mover
{
  public:
    void OnCollisionEnter(Entity other) override;
};

/**
 * \brief Every Interval seconds spawns a Projectile moving in the direction
 *        the spawner faces (Speed units per second, lives Lifetime seconds)
 */
class Spawner : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;

  private:
    float m_Timer = 0.0f;
    int m_Spawned = 0;
};
