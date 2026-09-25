//---------------------------------------------------------------------------------
// MIUnits.h
//---------------------------------------------------------------------------------
//
// Behaviour of every Metal Invasion object, one C++ script per kind. The
// original game used behaviour trees stored in a BlackBoard resource and one
// ECS system per concern; here each object's script holds its own small state
// machine and talks to the MetalInvasion scene script for shared data (path
// finding fields, move orders, crystal inventory).
//
//   MIUnit            health / damage / death, shared by everything that fights
//    ├ MIPlayerUnit   selection highlight + follows move orders
//    │  ├ MISoldier   shoots enemies in range (laser)
//    │  ├ MISupport   mines crystals in range
//    │  └ MITank      turret + explosive bullets
//    ├ MIEnemySoldier walks to the base, stops to shoot whatever is in range
//    ├ MIEnemyTank    walks to the base, turret shoots player units / the base
//    ├ MIBase         game over when destroyed
//    └ MIWall         blocks paths until destroyed
//   MICrystal, MIBullet, MIExplosion
//
#pragma once

#include "MIPrefabs.h"
#include "Scripting/Script.h"

class MetalInvasion;
class MITurret;

//-----------------------------------------------------------------------------
// Shared
//-----------------------------------------------------------------------------

class MIUnit : public Script
{
  public:
    void OnStart() override;

    /**
     * \brief Remove health, OnKilled() when it reaches 0 (once)
     */
    void Damage(float amount);

    float Health() const { return m_Health; }
    bool IsDead() const { return m_Dead; }
    virtual MI::Side GetSide() const = 0;

  protected:
    // Default: the object is removed from the scene
    virtual void OnKilled() { DestroySelf(); }

    MetalInvasion* Game() const;

    /**
     * \brief Nearest living object of `side` within `range` (the base counts
     *        when includeBase), NULL_ENTITY if none
     */
    Entity NearestOf(MI::Side side, float range, bool includeBase = true);
    // Nearest object with one of `tags` within `range` (dead units skipped)
    Entity NearestTagged(const std::vector<const char*>& tags, float range);

    // Damage another object (anything with an MIUnit script)
    void Hit(Entity target, float amount);
    void FireLaser(Entity target, const Vec3& color);

    // Move with the physics body (velocity) and face the direction
    void Walk(const Vec3& direction, float speed);
    void Halt();

    float DistanceTo(Entity other) const;

    // A destroyed tank: a (harmless) explosion, the cannon goes too
    void ExplodeWithTurret(MITurret& turret);

    float m_Health = 100.0f;
    float m_MaxHealth = 100.0f;
    bool m_Dead = false;
};

/**
 * \brief Cannon on top of a tank hull. A separate scene object (a Turret
 *        tagged model) that the hull's script keeps on top of it, aims and fires
 */
class MITurret
{
  public:
    void Attach(Entity hull, const std::string& hullName);
    // Stay on the hull and reload
    void Update(Entity hull, float deltaSeconds);
    // Turn towards `target` and, once aimed and reloaded, fire a bullet of `side`
    void FireAt(const Vec3& target, float deltaSeconds, MI::Side side);
    void Remove();

  private:
    void Follow(Entity hull);
    // Turn towards `target`, true once aimed
    bool Aim(const Vec3& target, float deltaSeconds);

    Entity m_Cannon = NULL_ENTITY;
    float m_Cooldown = 0.0f;
};

//-----------------------------------------------------------------------------
// Player side
//-----------------------------------------------------------------------------

class MIPlayerUnit : public MIUnit
{
  public:
    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;
    MI::Side GetSide() const override { return MI::Side::Player; }

    int Battalion() const { return m_Battalion; }
    void SetBattalion(int battalion) { m_Battalion = battalion; }
    bool IsSelected() const { return m_Selected; }
    void SetSelected(bool selected) { m_Selected = selected; }

  protected:
    // What the unit does besides walking (shoot, mine, ...)
    virtual void Act(float deltaSeconds) {}
    virtual float Speed() const { return 3.0f; }

  private:
    void FollowOrders();
    void UpdateHighlight();

    int m_Battalion = 0;
    bool m_Selected = false;
};

class MISoldier : public MIPlayerUnit
{
  protected:
    void Act(float deltaSeconds) override;

  private:
    float m_Reload = 0.0f;
};

class MISupport : public MIPlayerUnit
{
  protected:
    void Act(float deltaSeconds) override;

  private:
    float m_Mining = 0.0f;
};

class MITank : public MIPlayerUnit
{
  public:
    void OnStart() override;

  protected:
    void Act(float deltaSeconds) override;
    void OnKilled() override;

  private:
    MITurret m_Turret;
};

class MIBase : public MIUnit
{
  public:
    MI::Side GetSide() const override { return MI::Side::Player; }

  protected:
    void OnKilled() override;
};

class MIWall : public MIUnit
{
  public:
    MI::Side GetSide() const override { return MI::Side::Player; }

  protected:
    void OnKilled() override;
};

//-----------------------------------------------------------------------------
// Enemy side
//-----------------------------------------------------------------------------

class MIEnemySoldier : public MIUnit
{
  public:
    void OnUpdate(float deltaSeconds) override;
    MI::Side GetSide() const override { return MI::Side::Enemy; }

  private:
    float m_Reload = 0.0f;
};

class MIEnemyTank : public MIUnit
{
  public:
    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;
    MI::Side GetSide() const override { return MI::Side::Enemy; }

  protected:
    void OnKilled() override;

  private:
    MITurret m_Turret;
};

//-----------------------------------------------------------------------------
// World / effects
//-----------------------------------------------------------------------------

class MICrystal : public Script
{
  public:
    void OnStart() override;
    /**
     * \brief Take up to `amount` crystals, the deposit disappears when empty
     * \return crystals actually mined
     */
    int Mine(int amount);
    int Amount() const { return m_Amount; }

  private:
    int m_Amount = 0;
};

class MIBullet : public Script
{
  public:
    void OnUpdate(float deltaSeconds) override;
    void OnCollisionEnter(Entity other) override;

  private:
    void Explode();
    MI::Side Side() const;
    float m_Age = 0.0f;
    bool m_Exploded = false;
};

class MIExplosion : public Script
{
  public:
    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;

  private:
    float m_Age = 0.0f;
};
