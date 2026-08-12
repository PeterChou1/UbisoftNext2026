#pragma once
#include "BehaviorTree.h"
#include "Entity.h"

struct TurnTurretTowardTarget : Node
{
    Entity UnitId;
    float TurnSpeed = 0.0025f;
    bool isPlayerControlled = true;

    TurnTurretTowardTarget(Entity UnitId)
        : UnitId(UnitId)
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "Turn To Player Leaf"; }
};

struct TurnTurretBack : Node
{
    Entity TurretEntity;
    Entity AlignEntity;
    float TurnSpeed = 0.0025f;

    TurnTurretBack(Entity Enemy, Entity AlignEntity)
        : TurretEntity(Enemy)
        , AlignEntity(AlignEntity)
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "TurnTurretBack"; }
};

struct ShootTurret : Node
{

    Entity UnitId;
    bool isPlayerControlled = true;

    ShootTurret(Entity UnitId)
        : UnitId(UnitId)
    {
    }

    Status Process() override;

    std::string GetRunning() override { return "Shoot Player Leaf"; }
};