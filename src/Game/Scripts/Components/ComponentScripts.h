//---------------------------------------------------------------------------------
// ComponentScripts.h
//---------------------------------------------------------------------------------
//
// Scripts that read and write the example components (GameComponents.h):
// behaviour in scripts, data in components edited in the scene editor.
//
#pragma once

#include "Scripting/Script.h"

/**
 * \brief Walks along a path of Waypoint components. The object's own
 *        Waypoint::Next is the first point; each point's Next is the one after
 *        it, and its WaitSeconds the pause there. Speed: units per second
 */
class WaypointFollower : public Script
{
  public:
    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;

    Entity Target() const { return m_Target; }

  private:
    Entity m_Target = NULL_ENTITY;
    float m_Wait = 0.0f;
};

/**
 * \brief Trigger that removes Damage hit points from every object with a
 *        Health component entering it (unless Invulnerable), and destroys it
 *        at 0 when the Health says so
 */
class DamageZone : public Script
{
  public:
    void OnCollisionEnter(Entity other) override;

    /**
     * \brief Apply the damage to `target` (also called by tests)
     */
    void Hit(Entity target);
};
