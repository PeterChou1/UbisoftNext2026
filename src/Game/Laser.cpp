#include "Laser.h"

#include "ECSManager.h"
#include "GameUtils.h"
#include "app.h"

extern ECSManager ECS;

void CreateLaser(Vec3& Location, Quat& Rotation, float length)
{
    App::PlayAudio("data/Sounds/tankFire.wav");
    Entity E = CreateMeshEntity(Location, Laser, Rotation, {1, 1, length});
    ECS.AddComponent<LaserProjectile>(E, {0.2f});
}
