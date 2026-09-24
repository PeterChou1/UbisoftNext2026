#include "CreateMainLevel.h"

#include "AssetServer.h"
#include "Assets.h"
#include "BasicEnemyUnit.h"
#include "BlackBoard.h"
#include "ColliderCategory.h"
#include "ECSManager.h"
#include "GameState.h"
#include "GameUtils.h"
#include "Mesh.h"
#include "PlayerBase.h"
#include "PlayerUnits.h"
#include "Prefabs.h"
#include "RigidBody.h"
#include "UITarget.h"
#include "stdafx.h"

extern ECSManager ECS;

void LoadMainLevelAssets()
{
    auto& server = AssetServer::GetInstance();
    server.LoadLevelAssets({Ground,
                            PlayerBase,
                            SoldierUnitAsset,
                            TargetUISelector,
                            BasicEnemy,
                            SupportUnitAsset,
                            Laser,
                            CrystalAsset,
                            BaseTank,
                            CannonTank,
                            Bullet,
                            ExplosionBall,
                            ObstacleWall});
}

void CreateMainLevel()
{
    Vec3 worldOrigin = {0, 0, 0};
    auto Board = ECS.GetResource<BlackBoard>();
    LoadMainLevelAssets();
    // Add Ground Plane
    Prefabs::SpawnGround();
    // Add Base
    Prefabs::SpawnPlayerBase(worldOrigin);

    // Add Selector Target
    Entity Selector = Prefabs::SpawnUnitSelector({2, 2, 0});
    auto Obstacles = ECS.Visit<AIObstacle>();

    Board->UnitTarget = Selector;
    // Setup Vector Field
    Board->UnitVectorField.HalfWidth = 25.0;
    Board->UnitVectorField.HalfHeight = 25.0;
    Board->UnitVectorField.CreateVectorField(worldOrigin);

    Board->EnemyVectorField.HalfWidth = 25.0;
    Board->EnemyVectorField.HalfHeight = 25.0;
    Board->EnemyVectorField.CreateVectorField(worldOrigin);

    Board->SetMapObstacles(Obstacles);
}

namespace
{
    bool HasMesh(Entity E, ObjAsset Asset)
    {
        return ECS.HasComponent<Mesh>(E) && ECS.GetComponent<Mesh>(E).MeshType == Asset;
    }
} // namespace

void RestoreMainLevelRuntimeState()
{
    auto Board = ECS.GetResource<BlackBoard>();

    // -- Vector fields ---------------------------------------------------------
    // Only the field configuration is saved, rebuild the grids and re-apply
    // every obstacle (base, crystals, placed walls) of the restored world
    for (VectorField* Field : {&Board->UnitVectorField, &Board->EnemyVectorField})
    {
        Vec3 Location = Field->LocationVectorField;
        Field->ClearField();
        Field->CreateVectorField(Location);
    }
    auto Obstacles = ECS.Visit<AIObstacle>();
    Board->SetMapObstacles(Obstacles);

    // -- Unit selector ----------------------------------------------------------
    for (Entity Selector : ECS.Visit<UITarget>())
    {
        Board->UnitTarget = Selector;
        break;
    }

    // -- Behaviour trees ----------------------------------------------------------
    // Node graphs hold code and references and are not saved, rebuild them from
    // the unit type (identified by its components and mesh)
    Board->BehaviorTreeDataBase.clear();

    for (Entity Unit : ECS.Visit<PlayerControlUnit>())
    {
        const PlayerControlUnit& Control = ECS.GetComponent<PlayerControlUnit>(Unit);
        if (Control.isTank)
        {
            Entity Cannon = Prefabs::FindChildWithMesh(Unit, CannonTank);
            if (Cannon != NULL_ENTITY)
                AttachPlayerTankBehaviour(Unit, Cannon);
        }
        else if (HasMesh(Unit, SoldierUnitAsset))
            AttachSoldierBehaviour(Unit);
        else if (HasMesh(Unit, SupportUnitAsset))
            AttachSupportBehaviour(Unit);
        // Walls (ObstacleWall) have no AI
    }

    float DefaultEnemySpeed = ECS.GetResource<GameState>()->enemySpeedLower;
    for (Entity Unit : ECS.Visit<BasicEnemyUnit>())
    {
        if (HasMesh(Unit, BasicEnemy))
        {
            float Speed = ECS.GetComponent<BasicEnemyUnit>(Unit).Speed;
            AttachShootEnemyBehaviour(Unit, Speed > 0.0f ? Speed : DefaultEnemySpeed);
            continue;
        }
        Entity Cannon = Prefabs::FindChildWithMesh(Unit, CannonTank);
        if (Cannon != NULL_ENTITY)
            AttachEnemyTankBehaviour(Unit, Cannon);
    }
}
