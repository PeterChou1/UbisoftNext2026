#include "BasicEnemyUnit.h"

#include "AINodes.h"
#include "BlackBoard.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "GameUtils.h"
#include "Laser.h"
#include "PlayerBase.h"
#include "PlayerUnits.h"
#include "RigidBody.h"

extern ECSManager ECS;

struct DetectPlayerUnits : Node
{

    DetectPlayerUnits(Entity UnitId)
        : UnitId(UnitId){};

    bool CheckCondition()
    {

        Transform& T = ECS.GetComponent<Transform>(UnitId);
        Vec3 Pos = T.GetWorldPosition();
        Entity Base = *ECS.Visit<PlayerBaseComponent>().begin();
        Transform& BaseT = ECS.GetComponent<Transform>(Base);
        Vec3 BaseVec = BaseT.GetWorldPosition();
        if ((BaseVec - Pos).GetMagnitude() < FiringDistance)
        {
            TargetUnit = Base;
            return true;
        }

        for (Entity Soldier : ECS.Visit<PlayerControlUnit>())
        {
            Transform& SoldierT = ECS.GetComponent<Transform>(Soldier);
            Vec3 SoldierPos = SoldierT.GetWorldPosition();
            if ((SoldierPos - Pos).GetMagnitude() < FiringDistance)
            {
                TargetUnit = Soldier;
                return true;
            }
        }
        return false;
    }

    Status Process() override { return Running; }

    float FiringDistance = 5.0f;
    Entity TargetUnit = NULL_ENTITY;
    Entity UnitId;
};

struct ShootPlayerUnitsNearby : DetectPlayerUnits
{

    ShootPlayerUnitsNearby(Entity UnitId)
        : DetectPlayerUnits(UnitId){};

    Status Process() override
    {

        auto Board = ECS.GetResource<BlackBoard>();
        float deltaTime = Board->DeltaTime;
        CurrentTime -= (deltaTime / 1000);
        if (CurrentTime > 0.0f)
            return Running;

        CurrentTime = ReloadTime;
        Transform& Unit = ECS.GetComponent<Transform>(UnitId);
        Transform& Target = ECS.GetComponent<Transform>(TargetUnit);
        Vec3 UnitPosition = Unit.GetWorldPosition();
        Vec3 TargetPosition = Target.GetWorldPosition();
        Vec3 Displace = UnitPosition - TargetPosition;
        float Mag = Displace.GetMagnitude();
        float Angle = atan2(Displace.X, Displace.Z);
        Quat Q = Quat(Vec3(0, 1, 0), Angle + PI);
        CreateLaser(UnitPosition, Q, Mag);

        // Decrease Health of Player units
        if (ECS.HasComponent<PlayerBaseComponent>(TargetUnit))
        {
            ECS.GetComponent<PlayerBaseComponent>(TargetUnit).PlayerBaseHealth -= EnemyDamage;
        }
        else if (ECS.HasComponent<PlayerControlUnit>(TargetUnit))
        {
            ECS.GetComponent<PlayerControlUnit>(TargetUnit).health -= EnemyDamage;
        }
        return Success;
    }

    int EnemyDamage = 10;

    float CurrentTime = 0.0f;
    float ReloadTime = 3.0f;
};

struct FindTargetForEnemyTank : Node
{

    Entity UnitId;
    float FiringRange = 10.0f;

    FindTargetForEnemyTank(Entity UnitId)
        : UnitId(UnitId)
    {
    }

    Status Process() override
    {
        Transform& T = ECS.GetComponent<Transform>(UnitId);
        Vec3 Pos = T.GetWorldPosition();
        auto Board = ECS.GetResource<BlackBoard>();
        Entity Base = *ECS.Visit<PlayerBaseComponent>().begin();
        Transform& BaseT = ECS.GetComponent<Transform>(Base);
        Vec3 BaseVec = BaseT.GetWorldPosition();
        if ((BaseVec - Pos).GetMagnitude() < FiringRange)
        {
            Board->EnemyTankTargets[UnitId] = Base;
            return Success;
        }

        for (Entity Player : ECS.Visit<PlayerControlUnit>())
        {
            Transform& PlayerT = ECS.GetComponent<Transform>(Player);
            Vec3 PlayerPos = PlayerT.GetWorldPosition();
            if ((PlayerPos - Pos).GetMagnitude() < FiringRange)
            {
                Board->EnemyTankTargets[UnitId] = Player;
                return Success;
            }
        }
        return Running;
    }

    std::string GetRunning() override { return "Turn To Player Leaf"; }
};

Entity CreateEnemyTank(float x, float y, int health)
{
    Vec3 Location = {x, 0, y};

    Entity TankEntity = ECS.CreateEntity();
    Transform T = Transform(Location, Quat());
    T.Plane = XZ;
    ECS.AddComponent<Transform>(TankEntity, T);
    auto rigidbody = RigidBody(0.5f, 0.5f);
    rigidbody.Category = UnitCollider;

    ECS.AddComponent<RigidBody>(TankEntity, rigidbody);
    ECS.AddComponent<BasicEnemyUnit>(TankEntity, {200});
    ECS.AddComponent<FragShaderTag>(TankEntity, FragShaderTag(BlinnPhongID));

    Entity TankBaseEntity = CreateMeshEntity({0, 0, 0}, BaseTank, Quat(), {3, 3, 3});
    ECS.GetComponent<Transform>(TankBaseEntity).SetParentEntity(TankEntity, TankBaseEntity);
    Entity TankCannonEntity = CreateMeshEntity({0, 0, 0}, CannonTank, Quat(), {3, 3, 3});
    ECS.GetComponent<Transform>(TankCannonEntity).SetParentEntity(TankEntity, TankCannonEntity);

    AttachEnemyTankBehaviour(TankEntity, TankCannonEntity);

    return TankEntity;
}

void AttachEnemyTankBehaviour(Entity TankEntity, Entity TankCannonEntity)
{
    std::shared_ptr<BlackBoard> Board = ECS.GetResource<BlackBoard>();

    // --- AI ---
    // See documentation for full visualization and explanation of this Tree
    auto TankEnemyBehavior = std::make_unique<PrioritySelectorNode>();
    auto WaitIfUnitNear = std::make_shared<DetectPlayerUnits>(TankEntity);
    WaitIfUnitNear->FiringDistance = 10.0f;
    auto WalkToBase = std::make_shared<VectorFieldNode>(Board->EnemyVectorField, TankEntity);
    TankEnemyBehavior->AddChild(WaitIfUnitNear);
    TankEnemyBehavior->AddChild(WalkToBase);
    // Turret has a separate AI
    auto TurretRootBehaviour = std::make_unique<SequenceNode>();
    auto FindTarget = std::make_shared<FindTargetForEnemyTank>(TankCannonEntity);
    auto TurnTowardsPlayer = std::make_shared<TurnTurretTowardTarget>(TankCannonEntity);
    TurnTowardsPlayer->isPlayerControlled = false;
    auto ShootTurretNode = std::make_shared<ShootTurret>(TankCannonEntity);
    ShootTurretNode->isPlayerControlled = false;
    auto TurnTurretBackNode = std::make_shared<TurnTurretBack>(TankCannonEntity, TankEntity);
    auto WaitTurretNode = std::make_shared<Wait>(2.0f);

    TurretRootBehaviour->AddChild(FindTarget);
    TurretRootBehaviour->AddChild(TurnTowardsPlayer);
    TurretRootBehaviour->AddChild(ShootTurretNode);
    TurretRootBehaviour->AddChild(TurnTurretBackNode);
    TurretRootBehaviour->AddChild(WaitTurretNode);

    Board->AddBehaviouralTree(TankEntity, std::move(TankEnemyBehavior));
    Board->AddBehaviouralTree(TankCannonEntity, std::move(TurretRootBehaviour));
}

void AttachShootEnemyBehaviour(Entity Unit, float speed)
{
    std::shared_ptr<BlackBoard> Board = ECS.GetResource<BlackBoard>();

    auto ShootEnemyBehavior = std::make_unique<PrioritySelectorNode>();
    auto ShootUnitsNode = std::make_shared<ShootPlayerUnitsNearby>(Unit);
    auto WalkToTarget = std::make_shared<VectorFieldNode>(Board->EnemyVectorField, Unit);

    WalkToTarget->Speed = speed;
    WalkToTarget->StopDistance = 2.0f;

    ShootEnemyBehavior->AddChild(ShootUnitsNode);
    ShootEnemyBehavior->AddChild(WalkToTarget);
    Board->AddBehaviouralTree(Unit, std::move(ShootEnemyBehavior));
}

Entity CreateShootEnemyUnit(float x, float y, int health, float speed)
{
    // All enemy are programmed to walk towards enemy base
    Entity Unit = CreateMeshEntity({x, 0, y}, BasicEnemy);
    auto rigidbody = RigidBody(0.30, 0.30);
    rigidbody.Category = UnitCollider;

    ECS.AddComponent<BasicEnemyUnit>(Unit, {100, speed});
    ECS.AddComponent<RigidBody>(Unit, rigidbody);
    ECS.AddComponent<FragShaderTag>(Unit, FragShaderTag(BlinnPhongID));

    AttachShootEnemyBehaviour(Unit, speed);

    return Unit;
}

void CreateEnemyBattalion(float x, float y, int health, int unitcount, float speed)
{
    // Create unit spread out based on unit count
    if (unitcount <= 0)
        return;

    const float spacing = 0.5f; // distance between units
    const int unitsPerRow = static_cast<int>(std::ceil(std::sqrt(unitcount)));
    int created = 0;

    for (int row = 0; row < unitsPerRow && created < unitcount; ++row)
    {
        for (int col = 0; col < unitsPerRow && created < unitcount; ++col)
        {
            float offsetX = (col - (unitsPerRow - 1) * 0.5f) * spacing;
            float offsetY = (row - (unitsPerRow - 1) * 0.5f) * spacing;
            CreateShootEnemyUnit(x + offsetX, y + offsetY, health, speed);
            ++created;
        }
    }
}