#include "GameUtils.h"

#include "Map.h"
#include "Mesh.h"
#include "RigidBody.h"
#include "Transform.h"
#include "stdafx.h"

extern ECSManager ECS;

float WrapAngle(float angle)
{
    angle = std::fmod(angle + 3.141f, 2.0f * 3.141f);
    if (angle < 0.0f)
        angle += 2.0f * 3.141f;
    return angle - 3.141f;
}

float SmoothInterpolate(Vec3& Delta, Transform& T, float TurnSpeed, float DeltaTime)
{
    float DesiredAngle = atan2(Delta.X, Delta.Z);

    float CurrentAngle = T.GetWorldRotation().GetPitch2D();

    float maxTurnStep = TurnSpeed * DeltaTime;

    // Wrap the angle between -pi, pi

    float angleDiff = WrapAngle(DesiredAngle - CurrentAngle);
    // Clamp angleDiff to [-maxTurnStep, maxTurnStep]
    if (angleDiff > maxTurnStep)
        angleDiff = maxTurnStep;
    else if (angleDiff < -maxTurnStep)
        angleDiff = -maxTurnStep;

    float newAngle = CurrentAngle + angleDiff;
    Quat smoothRot = Quat(Vec3(0.0f, 1.0f, 0.0f), newAngle);

    T.Update(Delta, Quat());
    T.SetGlobalRotation(smoothRot);

    return angleDiff;
}

Entity CreateMeshEntity(Vec3 Position, ObjAsset meshID, Quat Rotate, Vec3 Scale)
{
    Entity E = ECS.CreateEntity();
    Transform T = Transform(Position);
    T.SetGlobalRotation(Rotate);
    T.Scale(Scale);
    T.Plane = XZ;
    ECS.AddComponent<Transform>(E, T);
    ECS.AddComponent<Mesh>(E, Mesh(meshID));
    return E;
}

Entity CreateRigidBodyRect(float x,
                           float y,
                           float width,
                           float height,
                           float rotation,
                           ColliderCategory category,
                           bool collidable,
                           bool AI)
{
    Entity entity = ECS.CreateEntity();

    float radians = (3.141f / 180.0f) * rotation;
    auto transform = Transform(Vec3(x, 0.0, y), Quat(Vec3(0.0f, 1.0f, 0.0f), radians));
    transform.Plane = XZ;

    auto rigidbody = RigidBody(width, height);
    rigidbody.SetStatic();
    rigidbody.Color = Vec3(1.0, 0.0, 0.0);
    rigidbody.Category = category;
    rigidbody.Collidable = collidable;

    ECS.AddComponent<Transform>(entity, transform);
    ECS.AddComponent<RigidBody>(entity, rigidbody);
    if (AI)
        ECS.AddComponent<AIObstacle>(entity, {width / 2 + 0.05f, height / 2 + 0.05f});
    return entity;
}
