#include "Prefabs.h"

#include "BasicEnemyUnit.h"
#include "Crystal.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "GameUtils.h"
#include "Map.h"
#include "Mesh.h"
#include "PlayerBase.h"
#include "PlayerUnits.h"
#include "RigidBody.h"
#include "UITarget.h"

extern ECSManager ECS;

namespace
{
    constexpr float HALF_PI = 3.14159265f * 0.5f;

    // Scale of the tank meshes (see the original CreateTank)
    const Vec3 TANK_MESH_SCALE = {3, 3, 3};

    // AI footprint of a placed wall (see BuildObstaclesSystem)
    const AIObstacle WALL_OBSTACLE = {1, 5};
    const AIObstacle WALL_OBSTACLE_FLIPPED = {5, 1};

    RigidBody UnitBody(float size)
    {
        RigidBody body(size, size);
        body.Category = UnitCollider;
        return body;
    }

    Entity SpawnTankRoot(const Vec3& position)
    {
        Entity tank = ECS.CreateEntity();
        Transform t = Transform(position, Quat());
        t.Plane = XZ;
        ECS.AddComponent<Transform>(tank, t);
        ECS.AddComponent<RigidBody>(tank, UnitBody(0.5f));
        return tank;
    }

    void AddTankMeshes(Entity tank)
    {
        Entity base = CreateMeshEntity({0, 0, 0}, BaseTank, Quat(), TANK_MESH_SCALE);
        ECS.GetComponent<Transform>(base).SetParentEntity(tank, base);
        Entity cannon = CreateMeshEntity({0, 0, 0}, CannonTank, Quat(), TANK_MESH_SCALE);
        ECS.GetComponent<Transform>(cannon).SetParentEntity(tank, cannon);
    }
} // namespace

namespace Prefabs
{
    Entity SpawnGround()
    {
        return CreateMeshEntity({0, 0, 0}, Ground);
    }

    Entity SpawnPlayerBase(const Vec3& position, int health)
    {
        Entity base = CreateMeshEntity(position, PlayerBase);
        RigidBody body(2.5f, 2.5f);
        body.SetStatic();
        body.Category = UnitCollider;
        ECS.AddComponent<RigidBody>(base, body);
        ECS.AddComponent<PlayerBaseComponent>(base, {health});
        ECS.AddComponent<FragShaderTag>(base, FragShaderTag(BlinnPhongID));
        ECS.AddComponent<AIObstacle>(base, {3, 3});
        return base;
    }

    Entity SpawnUnitSelector(const Vec3& position)
    {
        Entity selector = CreateMeshEntity(position, TargetUISelector);
        ECS.AddComponent<UITarget>(selector, {false});
        return selector;
    }

    Entity SpawnSoldier(const Vec3& position, int health, int battalionId)
    {
        Entity unit = CreateMeshEntity(position, SoldierUnitAsset);
        ECS.AddComponent<PlayerControlUnit>(unit, {battalionId, health, false});
        ECS.AddComponent<RigidBody>(unit, UnitBody(0.30f));
        ECS.AddComponent<FragShaderTag>(unit, FragShaderTag(BlinnPhongID));
        return unit;
    }

    Entity SpawnSupport(const Vec3& position, int health, int battalionId)
    {
        Entity unit = CreateMeshEntity(position, SupportUnitAsset);
        ECS.AddComponent<PlayerControlUnit>(unit, {battalionId, health, false});
        ECS.AddComponent<RigidBody>(unit, UnitBody(0.30f));
        ECS.AddComponent<FragShaderTag>(unit, FragShaderTag(BlinnPhongID));
        return unit;
    }

    Entity SpawnPlayerTank(const Vec3& position, int battalionId, int health)
    {
        Entity tank = SpawnTankRoot(position);
        ECS.AddComponent<PlayerControlUnit>(tank, {battalionId, health, false, true});
        ECS.AddComponent<FragShaderTag>(tank, FragShaderTag(BlinnPhongID));
        AddTankMeshes(tank);
        return tank;
    }

    Entity SpawnEnemySoldier(const Vec3& position, int health, float speed)
    {
        Entity unit = CreateMeshEntity(position, BasicEnemy);
        ECS.AddComponent<BasicEnemyUnit>(unit, {health, speed});
        ECS.AddComponent<RigidBody>(unit, UnitBody(0.30f));
        ECS.AddComponent<FragShaderTag>(unit, FragShaderTag(BlinnPhongID));
        return unit;
    }

    Entity SpawnEnemyTank(const Vec3& position, int health)
    {
        Entity tank = SpawnTankRoot(position);
        ECS.AddComponent<BasicEnemyUnit>(tank, {health});
        ECS.AddComponent<FragShaderTag>(tank, FragShaderTag(BlinnPhongID));
        AddTankMeshes(tank);
        return tank;
    }

    Entity SpawnCrystal(const Vec3& position, int amount)
    {
        Entity crystal = CreateMeshEntity(position, CrystalAsset);
        ECS.AddComponent<CrystalDeposit>(crystal, {amount});
        ECS.AddComponent<AIObstacle>(crystal, {0.5f, 0.5f});
        return crystal;
    }

    Entity SpawnWall(const Vec3& position, bool flipped)
    {
        Entity wall = CreateMeshEntity(position, ObstacleWall);
        RigidBody body(1.0f, 10.0f);
        body.Category = UnitCollider;
        body.SetStatic();
        ECS.AddComponent<RigidBody>(wall, body);
        ECS.AddComponent<PlayerControlUnit>(wall, {-1, 250, false});
        ECS.AddComponent<FragShaderTag>(wall, FragShaderTag(BlinnPhongID));
        ECS.AddComponent<AIObstacle>(wall, flipped ? WALL_OBSTACLE_FLIPPED : WALL_OBSTACLE);
        if (flipped)
            ECS.GetComponent<Transform>(wall).SetGlobalRotation(Quat({0, 1, 0}, HALF_PI));
        return wall;
    }

    Entity FindChildWithMesh(Entity parent, ObjAsset asset)
    {
        if (!ECS.HasComponent<Transform>(parent))
            return NULL_ENTITY;
        for (Entity child : ECS.GetComponent<Transform>(parent).Children)
        {
            if (ECS.IsEntityAlive(child) && ECS.HasComponent<Mesh>(child) &&
                ECS.GetComponent<Mesh>(child).MeshType == asset)
            {
                return child;
            }
        }
        return NULL_ENTITY;
    }

    void SetWallFlipped(Entity wall, bool flipped)
    {
        ECS.GetComponent<Transform>(wall).SetGlobalRotation(flipped ? Quat({0, 1, 0}, HALF_PI)
                                                                    : Quat());
        ECS.GetComponent<AIObstacle>(wall) = flipped ? WALL_OBSTACLE_FLIPPED : WALL_OBSTACLE;
    }
} // namespace Prefabs
