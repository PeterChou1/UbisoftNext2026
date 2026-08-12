#include "BulletColliders.h"

#include "BasicEnemyUnit.h"
#include "Bullet.h"
#include "PlayerBase.h"
#include "PlayerUnits.h"
#include "stdafx.h"

extern ECSManager ECS;

void BulletUnitCollider::OnCollide(Entity self, Entity other, RigidBody& selfRB, RigidBody& otherRB)
{
    auto& T = ECS.GetComponent<Transform>(self);
    CreateExplosion(T.GetWorldPosition());
    ECS.DestroyEntity(self);
}

void ExplosionUnitCollider::OnCollide(Entity self,
                                      Entity other,
                                      RigidBody& selfRB,
                                      RigidBody& otherRB)
{
    if (ECS.HasComponent<PlayerControlUnit>(other))
    {
        ECS.GetComponent<PlayerControlUnit>(other).health -= 1;
    }
    else if (ECS.HasComponent<BasicEnemyUnit>(other))
    {
        ECS.GetComponent<BasicEnemyUnit>(other).health -= 1;
    }
    else if (ECS.HasComponent<PlayerBaseComponent>(other))
    {
        ECS.GetComponent<PlayerBaseComponent>(other).PlayerBaseHealth -= 1;
    }
}
