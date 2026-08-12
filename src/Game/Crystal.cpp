#include "Crystal.h"

#include "ECSManager.h"
#include "GameUtils.h"
#include "Map.h"

extern ECSManager ECS;

void CreateCrystal(Vec3& Location)
{
    Entity E = CreateMeshEntity(Location, CrystalAsset);
    ECS.AddComponent<CrystalDeposit>(E, {30});
    ECS.AddComponent<AIObstacle>(E, {0.5f, 0.5f});
}
