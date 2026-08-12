#include "CreateMainLevel.h"

#include "AssetServer.h"
#include "Assets.h"
#include "BlackBoard.h"
#include "ColliderCategory.h"
#include "ECSManager.h"
#include "GameUtils.h"
#include "PlayerBase.h"
#include "RigidBody.h"
#include "UITarget.h"
#include "stdafx.h"

extern ECSManager ECS;

void CreateMainLevel()
{
    Vec3 worldOrigin = {0, 0, 0};
    auto& server = AssetServer::GetInstance();
    auto Board = ECS.GetResource<BlackBoard>();
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
    // Add Ground Plane
    CreateMeshEntity(worldOrigin, Ground);
    // Add Base
    Entity Base = CreateMeshEntity(worldOrigin, PlayerBase);
    auto rigidbody = RigidBody(2.5f, 2.5f);
    rigidbody.SetStatic();
    rigidbody.Category = UnitCollider;
    ECS.AddComponent<RigidBody>(Base, rigidbody);
    ECS.AddComponent<PlayerBaseComponent>(Base, {1000});
    ECS.AddComponent<FragShaderTag>(Base, FragShaderTag(BlinnPhongID));
    ECS.AddComponent<AIObstacle>(Base, {3, 3});

    // Add Selector Target
    Entity Selector = CreateMeshEntity({2, 2, 0}, TargetUISelector);
    auto Obstacles = ECS.Visit<AIObstacle>();

    ECS.AddComponent<UITarget>(Selector, {false});
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