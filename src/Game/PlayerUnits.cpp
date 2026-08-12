#include "PlayerUnits.h"

#include "AINodes.h"
#include "BasicEnemyUnit.h"
#include "BlackBoard.h"
#include "Crystal.h"
#include "ECSManager.h"
#include "FragShaderTag.h"
#include "GameState.h"
#include "GameUtils.h"
#include "Laser.h"
#include "RigidBody.h"
#include "UITarget.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

struct ShootEnemyUnitsNearby : Node
{

    ShootEnemyUnitsNearby(Entity UnitId)
        : UnitId(UnitId){};

    Status Process() override
    {

        Transform& T = ECS.GetComponent<Transform>(UnitId);
        Vec3 Pos = T.GetWorldPosition();
        bool HasTarget = false;

        for (Entity Soldier : ECS.Visit<BasicEnemyUnit>())
        {
            Transform& SoldierT = ECS.GetComponent<Transform>(Soldier);
            Vec3 SoldierPos = SoldierT.GetWorldPosition();
            if ((SoldierPos - Pos).GetMagnitude() < FiringDistance)
            {
                TargetUnit = Soldier;
                HasTarget = true;
                break;
            }
        }

        if (!HasTarget)
            return Running;

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

        if (ECS.HasComponent<BasicEnemyUnit>(TargetUnit))
        {
            auto& B = ECS.GetComponent<BasicEnemyUnit>(TargetUnit);
            B.health -= UnitDamage;
        }

        return Success;
    }

    int UnitDamage = 20;
    float FiringDistance = 5.0f;
    float CurrentTime = 0.0f;
    float ReloadTime = 2.0f;
    Entity TargetUnit;
    Entity UnitId;
};

struct MineCrystal : Node
{

    MineCrystal(Entity UnitId)
        : UnitId(UnitId){};

    Status Process() override
    {

        Transform& T = ECS.GetComponent<Transform>(UnitId);
        Vec3 Pos = T.GetWorldPosition();
        bool HasTarget = false;

        for (Entity Cryst : ECS.Visit<CrystalDeposit>())
        {
            Transform& CrystT = ECS.GetComponent<Transform>(Cryst);
            Vec3 CrystPos = CrystT.GetWorldPosition();
            if ((CrystPos - Pos).GetMagnitude() < MiningDistance)
            {
                HasTarget = true;
                TargetUnit = Cryst;
                break;
            }
        }

        if (!HasTarget)
            return Running;

        auto Board = ECS.GetResource<BlackBoard>();
        float deltaTime = Board->DeltaTime;
        CurrentTime -= (deltaTime / 1000);
        if (CurrentTime > 0.0f)
            return Running;

        CurrentTime = MiningTime;

        CrystalDeposit& Deposit = ECS.GetComponent<CrystalDeposit>(TargetUnit);
        auto State = ECS.GetResource<GameState>();

        State->PlayerCrystalInventory += MinedAmount;
        Deposit.AmountOfCrystal -= MinedAmount;
        App::PlayAudio("data/Sounds/PickaxeSound.wav");
    }

    int MinedAmount = 1;
    float CurrentTime = 0.0f;
    float MiningTime = 2.0f;
    float MiningDistance = 2.0f;
    Entity TargetUnit;
    Entity UnitId;
};

struct WaitForTarget : Node
{

    WaitForTarget(Entity UnitId)
        : UnitId(UnitId){};

    Status Process() override
    {

        if (Children.size() == 0)
            return Failure;

        std::shared_ptr<BlackBoard> Board = ECS.GetResource<BlackBoard>();
        auto Unit = ECS.GetComponent<PlayerControlUnit>(UnitId);
        // Only move unit if its selected
        if (ECS.GetComponent<UITarget>(Board->UnitTarget).active && Unit.selected)
            return Children[0]->Process();

        return Failure;
    }

    Entity UnitId;
};

Entity CreateSoldierUnits(float x, float y, int health, int battalionId)
{
    std::shared_ptr<BlackBoard> Board = ECS.GetResource<BlackBoard>();
    Entity Unit = CreateMeshEntity({x, 0, y}, SoldierUnitAsset);
    auto rigidbody = RigidBody(0.30, 0.30);
    rigidbody.Category = UnitCollider;

    ECS.AddComponent<PlayerControlUnit>(Unit, {battalionId, health, false});
    ECS.AddComponent<RigidBody>(Unit, rigidbody);
    ECS.AddComponent<FragShaderTag>(Unit, FragShaderTag(BlinnPhongID));

    auto SoldierBehavior = std::make_unique<ConcurrentNode>();
    auto ShootEnemy = std::make_shared<ShootEnemyUnitsNearby>(Unit);
    auto FollowTargetSequence = std::make_shared<SequenceNode>();
    auto Wait = std::make_shared<WaitForTarget>(Unit);
    auto WalkToTarget = std::make_shared<VectorFieldNode>(Board->UnitVectorField, Unit);
    // auto UnselectUnit = std::make_shared<UnselectActiveUnit>(Unit);

    Wait->AddChild(WalkToTarget);
    FollowTargetSequence->AddChild(Wait);
    // FollowTargetSequence->AddChild(UnselectUnit);
    SoldierBehavior->AddChild(ShootEnemy);
    SoldierBehavior->AddChild(FollowTargetSequence);

    Board->AddBehaviouralTree(Unit, std::move(SoldierBehavior));

    return Unit;
}

Entity CreateSupportUnits(float x, float y, int health, int battalionId)
{
    std::shared_ptr<BlackBoard> Board = ECS.GetResource<BlackBoard>();
    Entity Unit = CreateMeshEntity({x, 0, y}, SupportUnitAsset);
    auto rigidbody = RigidBody(0.30, 0.30);
    rigidbody.Category = UnitCollider;

    ECS.AddComponent<PlayerControlUnit>(Unit, {battalionId, health, false});
    ECS.AddComponent<RigidBody>(Unit, rigidbody);
    ECS.AddComponent<FragShaderTag>(Unit, FragShaderTag(BlinnPhongID));

    auto SupportBehavior = std::make_unique<ConcurrentNode>();
    auto MineCrystalAction = std::make_shared<MineCrystal>(Unit);
    auto FollowTargetSequence = std::make_shared<SequenceNode>();
    auto Wait = std::make_shared<WaitForTarget>(Unit);
    auto WalkToTarget = std::make_shared<VectorFieldNode>(Board->UnitVectorField, Unit);

    Wait->AddChild(WalkToTarget);
    FollowTargetSequence->AddChild(Wait);
    SupportBehavior->AddChild(MineCrystalAction);
    SupportBehavior->AddChild(FollowTargetSequence);
    Board->AddBehaviouralTree(Unit, std::move(SupportBehavior));

    return Unit;
}

struct FindTargetForTank : Node
{

    Entity UnitId;
    float FiringRange = 10.0f;

    FindTargetForTank(Entity UnitId)
        : UnitId(UnitId)
    {
    }

    Status Process() override
    {
        Transform& T = ECS.GetComponent<Transform>(UnitId);
        Vec3 Pos = T.GetWorldPosition();
        auto Board = ECS.GetResource<BlackBoard>();

        for (Entity Enemy : ECS.Visit<BasicEnemyUnit>())
        {
            Transform& EnemyT = ECS.GetComponent<Transform>(Enemy);
            Vec3 EnemyPos = EnemyT.GetWorldPosition();
            if ((EnemyPos - Pos).GetMagnitude() < FiringRange)
            {
                Board->PlayerTankTargets[UnitId] = Enemy;
                return Success;
            }
        }
        return Running;
    }

    std::string GetRunning() override { return "Turn To Player Leaf"; }
};

void CreateTank(float x, float y, int battalionId)
{

    Vec3 Location = {x, 0, y};
    std::shared_ptr<BlackBoard> Board = ECS.GetResource<BlackBoard>();

    Entity TankEntity = ECS.CreateEntity();
    Transform T = Transform(Location, Quat());
    T.Plane = XZ;
    ECS.AddComponent<Transform>(TankEntity, T);
    auto rigidbody = RigidBody(0.5f, 0.5f);
    rigidbody.Category = UnitCollider;

    ECS.AddComponent<RigidBody>(TankEntity, rigidbody);
    ECS.AddComponent<PlayerControlUnit>(TankEntity, {battalionId, 200, false, true});
    ECS.AddComponent<FragShaderTag>(TankEntity, FragShaderTag(BlinnPhongID));

    Entity TankBaseEntity = CreateMeshEntity({0, 0, 0}, BaseTank, Quat(), {3, 3, 3});
    ECS.GetComponent<Transform>(TankBaseEntity).SetParentEntity(TankEntity, TankBaseEntity);
    Entity TankCannonEntity = CreateMeshEntity({0, 0, 0}, CannonTank, Quat(), {3, 3, 3});
    ECS.GetComponent<Transform>(TankCannonEntity).SetParentEntity(TankEntity, TankCannonEntity);

    // --- AI ---
    // See documentation for full visualization and explanation of this Tree
    auto TankRootBehaviour = std::make_unique<SequenceNode>();
    auto WaitForDir = std::make_shared<WaitForTarget>(TankEntity);
    auto WalkToTarget = std::make_shared<VectorFieldNode>(Board->UnitVectorField, TankEntity);
    // auto UnselectUnit = std::make_shared<UnselectActiveUnit>(TankEntity);

    WaitForDir->AddChild(WalkToTarget);
    TankRootBehaviour->AddChild(WaitForDir);
    // TankRootBehaviour->AddChild(UnselectUnit);

    // Turret has a separate AI
    auto TurretRootBehaviour = std::make_unique<SequenceNode>();

    auto FindTarget = std::make_shared<FindTargetForTank>(TankCannonEntity);
    auto TurnTowardsPlayer = std::make_shared<TurnTurretTowardTarget>(TankCannonEntity);
    auto ShootTurretNode = std::make_shared<ShootTurret>(TankCannonEntity);
    auto TurnTurretBackNode = std::make_shared<TurnTurretBack>(TankCannonEntity, TankEntity);
    auto WaitTurretNode = std::make_shared<Wait>(2.0f);

    TurretRootBehaviour->AddChild(FindTarget);
    TurretRootBehaviour->AddChild(TurnTowardsPlayer);
    TurretRootBehaviour->AddChild(ShootTurretNode);
    TurretRootBehaviour->AddChild(TurnTurretBackNode);
    TurretRootBehaviour->AddChild(WaitTurretNode);

    Board->AddBehaviouralTree(TankEntity, std::move(TankRootBehaviour));
    Board->AddBehaviouralTree(TankCannonEntity, std::move(TurretRootBehaviour));
}

void CreateSoldierBattalion(float x, float y, int health, int unitcount, int battlionId)
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

            CreateSoldierUnits(x + offsetX, y + offsetY, health, battlionId);

            ++created;
        }
    }
}

void CreateSupportBattalion(float x, float y, int health, int unitcount, int battlionId)
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

            CreateSupportUnits(x + offsetX, y + offsetY, health, battlionId);

            ++created;
        }
    }
}
