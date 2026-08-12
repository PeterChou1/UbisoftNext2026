#pragma once
#include "Quat.h"
#include "Vec3.h"

void CreateLaser(Vec3& Location, Quat& Rotation, float length);

struct LaserProjectile
{
    // How long laser stays out for
    float LaserTime;
};