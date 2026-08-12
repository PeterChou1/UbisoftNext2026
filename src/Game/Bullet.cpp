#include "Bullet.h"

#include "ECSManager.h"
#include "Emitter.h"
#include "FragShaderTag.h"
#include "GameUtils.h"
#include "Mesh.h"
#include "RigidBody.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

void CreateBullet(Vec3& Location, Quat& Rotation)
{
    App::PlayAudio("data/Sounds/tankFire.wav");
    float Speed = 0.5f;
    Vec3 Impulse = Vec3(0, 0, 0.5f);
    Impulse = Rotation.RotatePoint(Impulse);
    Vec3 Displace = Location + Impulse;
    Entity BulletEntity = CreateMeshEntity(Displace, Bullet, Rotation, {3, 3, 3});
    RigidBody R = RigidBody(0.15f, 0.15f);
    R.Category = BulletCollider;
    R.DynamicFriction = 0.0f;
    Vec2 Impulse2D = Vec2(Impulse.X, Impulse.Z) * Speed;
    R.ApplyImpulse(Impulse2D);
    ECS.AddComponent<RigidBody>(BulletEntity, R);
    ECS.AddComponent<TankBullet>(BulletEntity, {2.0f});
    // Create an Emission effect
    Entity BulletEmitter = ECS.CreateEntity();
    Emitter Emit;
    Emit.emitterType = Cone;
    Emit.density = 5;
    Emit.direction = Impulse;
    Emit.speed = 0.5f;
    Emit.size = 0.2f;
    Emit.particleTime = 1.5f;
    Emit.duration = 0.2f;
    Emit.color = Vec3(1.0f, 0.0, 0);
    Emit.coneAngle = 60.0f;
    ECS.AddComponent<Transform>(BulletEmitter, Transform(Displace));
    ECS.AddComponent<Emitter>(BulletEmitter, Emit);
}

void CreateExplosion(Vec3 Position)
{
    App::PlayAudio("data/Sounds/explosionSound.wav");
    Entity ExplosionEntity = CreateMeshEntity(Position, ExplosionBall);
    RigidBody R = RigidBody(0.5);
    R.Collidable = false;
    R.Category = ExplosionCollider;
    ECS.AddComponent<FragShaderTag>(ExplosionEntity, FragShaderTag(OutlineShaderID));
    ECS.AddComponent<RigidBody>(ExplosionEntity, R);
    ECS.AddComponent<Explosion>(ExplosionEntity, {1.5f, 0.25f});
}

void HandleTankProjectiles::Update(float deltaTime)
{
    float deltaSecond = deltaTime / 1000.0f;
    float explosionTurnSpeed = 3.0f;

    for (auto e : ECS.Visit<Transform, TankBullet>())
    {
        auto& Bullet = ECS.GetComponent<TankBullet>(e);
        Bullet.fuseTime -= deltaSecond;
        if (Bullet.fuseTime <= 0.0f)
        {
            auto& T = ECS.GetComponent<Transform>(e);
            CreateExplosion(T.GetWorldPosition());
            ECS.DestroyEntity(e);
        }
    }

    for (auto e : ECS.Visit<Transform, Explosion, RigidBody>())
    {
        Transform& T = ECS.GetComponent<Transform>(e);
        RigidBody& Rb = ECS.GetComponent<RigidBody>(e);
        Explosion& Ex = ECS.GetComponent<Explosion>(e);
        // make the explosion spin to make it interesting
        T.Update({0, 0, 0}, Quat({0, 1, 0}, explosionTurnSpeed * deltaSecond));
        Ex.Duration -= deltaSecond;
        T.Scale(1.0f + Ex.GrowthFactor * deltaSecond);
        float NewRadius = Rb.Shape.Radius + Ex.GrowthFactor * deltaSecond;
        Rb.UpdateRadius(NewRadius);
        if (Ex.Duration < 0.0f)
            ECS.DestroyEntity(e);
    }
}
