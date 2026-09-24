#include "Crystal.h"

#include "ECSManager.h"
#include "GameUtils.h"
#include "Map.h"
#include "Prefabs.h"

extern ECSManager ECS;

void CreateCrystal(Vec3& Location)
{
    Prefabs::SpawnCrystal(Location, 30);
}
