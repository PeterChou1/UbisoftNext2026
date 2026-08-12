#pragma once
#include "RigidBody.h"

class ExplosionUnitCollider : public Collider
{
  public:
    ExplosionUnitCollider()
        : Collider(ExplosionCollider, UnitCollider)
    {
    }

    ~ExplosionUnitCollider() override = default;

    void OnCollide(Entity self, Entity other, RigidBody& selfRB, RigidBody& otherRB) override;
};

class BulletUnitCollider : public Collider
{
  public:
    BulletUnitCollider()
        : Collider(BulletCollider, UnitCollider)
    {
    }

    ~BulletUnitCollider() override = default;

    void OnCollide(Entity self, Entity other, RigidBody& selfRB, RigidBody& otherRB) override;
};