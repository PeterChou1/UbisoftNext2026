//---------------------------------------------------------------------------------
// MovementScripts.h
//---------------------------------------------------------------------------------
//
// Object scripts that move things around. Attach them to objects in the scene
// editor (inspector -> Script) and tune their parameters there.
//
#pragma once

#include "Scripting/Script.h"

/**
 * \brief Spins the object around the up axis
 *        Speed: degrees per second
 */
class Rotator : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;
};

/**
 * \brief Moves back and forth along the direction the object faces
 *        Distance: how far from the start point, Speed: units per second
 */
class Patrol : public Script
{
  public:
    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;

  private:
    Vec3 m_Origin;
    Vec3 m_Direction;
    float m_Travelled = 0.0f;
};

/**
 * \brief Keeps moving in the direction it faces, destroyed after Lifetime
 *        seconds or when leaving the field. Speed: units per second
 */
class Mover : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;

  private:
    float m_Age = 0.0f;
};

/**
 * \brief Moves the object with WASD / arrow keys. Dynamic bodies are moved by
 *        velocity (they collide with walls), other objects directly.
 *        Speed: units per second
 */
class PlayerController : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;
};

/**
 * \brief Chases the object tagged "Player" when it is within Range
 *        Speed: units per second, Range: detection distance
 */
class Follower : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;
};
