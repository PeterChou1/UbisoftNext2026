#pragma once
#include "Quat.h"
#include "Vec3.h"

struct Explosion
{
    // How long the explosion will last
    float Duration;
    // How fast the explosion will grow
    float GrowthFactor;
};

struct TankBullet
{
    // if Bullet hits nothing for 2  sec explode automatically
    float fuseTime = 2.0f;
};

void CreateBullet(Vec3& Location, Quat& Rotation);

void CreateExplosion(Vec3 Position);

class HandleTankProjectiles
{
  public:
    void Update(float deltaTime);
};