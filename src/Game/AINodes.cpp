#include "AINodes.h"

#include "BlackBoard.h"
#include "Bullet.h"
#include "ECSManager.h"
#include "GameUtils.h"

extern ECSManager ECS;

Status TurnTurretTowardTarget::Process()
{

    auto Board = ECS.GetResource<BlackBoard>();

    if (isPlayerControlled &&
        Board->PlayerTankTargets.find(UnitId) == Board->PlayerTankTargets.end())
        return Failure;
    else if (!isPlayerControlled &&
             Board->EnemyTankTargets.find(UnitId) == Board->EnemyTankTargets.end())
        return Failure;

    Entity TargetId;

    if (isPlayerControlled)
        TargetId = Board->PlayerTankTargets[UnitId];
    else
        TargetId = Board->EnemyTankTargets[UnitId];

    if (!ECS.HasComponent<Transform>(TargetId))
        return Failure;

    Transform& UnitT = ECS.GetComponent<Transform>(UnitId);
    Transform& Target = ECS.GetComponent<Transform>(TargetId);

    Vec3 AngleVec = Target.GetWorldPosition() - UnitT.GetWorldPosition();
    // Turn the turret
    float CurrentAngle = UnitT.GetWorldRotation().GetPitch2D();
    float DesiredAngle = atan2(AngleVec.X, AngleVec.Z);

    // Shoot the turret
    float maxTurnStep = TurnSpeed * Board->DeltaTime;
    // Wrap the angle between -pi, pi
    float angleDiff = WrapAngle(DesiredAngle - CurrentAngle);

    if (std::abs(angleDiff) <= 0.1f)
        return Success;

    // Clamp angleDiff to [-maxTurnStep, maxTurnStep]
    if (angleDiff > maxTurnStep)
        angleDiff = maxTurnStep;
    else if (angleDiff < -maxTurnStep)
        angleDiff = -maxTurnStep;

    float newAngle = CurrentAngle + angleDiff;
    Quat smoothRot = Quat(Vec3(0.0f, 1.0f, 0.0f), newAngle);
    UnitT.SetGlobalRotation(smoothRot);
    return Running;
}

Status TurnTurretBack::Process()
{
    Transform& Enemy = ECS.GetComponent<Transform>(TurretEntity);
    Transform& Base = ECS.GetComponent<Transform>(AlignEntity);

    float CurrentAngle = Enemy.GetWorldRotation().GetPitch2D();
    float DesiredAngle = Base.GetWorldRotation().GetPitch2D();
    ;
    auto Board = ECS.GetResource<BlackBoard>();

    float maxTurnStep = TurnSpeed * Board->DeltaTime;
    // Wrap the angle between -pi, pi
    float angleDiff = WrapAngle(DesiredAngle - CurrentAngle);

    if (std::abs(angleDiff) <= 0.1f)
        return Success;

    // Clamp angleDiff to [-maxTurnStep, maxTurnStep]
    if (angleDiff > maxTurnStep)
        angleDiff = maxTurnStep;
    else if (angleDiff < -maxTurnStep)
        angleDiff = -maxTurnStep;

    float newAngle = CurrentAngle + angleDiff;
    Quat smoothRot = Quat(Vec3(0.0f, 1.0f, 0.0f), newAngle);
    Enemy.SetGlobalRotation(smoothRot);
    return Running;
}

Status ShootTurret::Process()
{
    auto Board = ECS.GetResource<BlackBoard>();
    if (isPlayerControlled &&
        Board->PlayerTankTargets.find(UnitId) == Board->PlayerTankTargets.end())
        return Failure;
    else if (!isPlayerControlled &&
             Board->EnemyTankTargets.find(UnitId) == Board->EnemyTankTargets.end())
        return Failure;

    Entity TargetId;

    if (isPlayerControlled)
        TargetId = Board->PlayerTankTargets[UnitId];
    else
        TargetId = Board->EnemyTankTargets[UnitId];

    Transform& Target = ECS.GetComponent<Transform>(TargetId);
    Transform& Unit = ECS.GetComponent<Transform>(UnitId);

    Vec3 TargetPos = Target.GetWorldPosition();
    Vec3 UnitPosition = Unit.GetWorldPosition();
    Vec3 AngleVec = TargetPos - UnitPosition;
    float Angle = atan2(AngleVec.X, AngleVec.Z);
    Quat Q = Quat(Vec3(0, 1, 0), Angle);
    UnitPosition.Y = 0.3f;

    CreateBullet(UnitPosition, Q);

    return Success;
}