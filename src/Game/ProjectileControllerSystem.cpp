#include "ProjectileControllerSystem.h"

#include "ECSManager.h"
#include "Laser.h"

extern ECSManager ECS;

ProjectileControllerSystem::ProjectileControllerSystem()
{
    m_TankProjectileHandle = std::make_shared<HandleTankProjectiles>();
}

void ProjectileControllerSystem::Update(float deltaTime)
{
    m_TankProjectileHandle->Update(deltaTime);
    for (Entity E : ECS.Visit<LaserProjectile>())
    {
        LaserProjectile& L = ECS.GetComponent<LaserProjectile>(E);
        L.LaserTime -= (deltaTime / 1000.0f);
        if (L.LaserTime < 0.0)
            ECS.DestroyEntity(E);
    }
}
