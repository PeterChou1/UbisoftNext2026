//---------------------------------------------------------------------------------
// GameWorldFixture.h
//---------------------------------------------------------------------------------
//
// Helpers shared by the serialization tests:
//   - BuildSampleLevel(): builds a mid-game Metal Invasion world (base, player
//     battalions, tanks with child entities, enemies, crystals, projectiles,
//     particles, an obstacle being placed, round state, AI memory ...)
//   - WorldImage: an independent, field by field copy of every component in the
//     world. Two images are compared with explicit comparison functions so the
//     tests never rely on the serializer to verify itself
//
#pragma once

#include "Serialization/GameSerialization.h"
#include "Serialization/WorldSerializer.h"
#include "TestFramework.h"

#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

extern ECSManager ECS;

namespace Fixture
{
    //-----------------------------------------------------------------------------
    // Bit exact comparisons (a save/load round trip must not change any bit)
    //-----------------------------------------------------------------------------

    inline bool Same(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

    inline bool Same(const Vec2& a, const Vec2& b) { return Same(a.X, b.X) && Same(a.Y, b.Y); }

    inline bool Same(const Vec3& a, const Vec3& b)
    {
        return Same(a.X, b.X) && Same(a.Y, b.Y) && Same(a.Z, b.Z);
    }

    inline bool Same(const Vec4& a, const Vec4& b)
    {
        return Same(a.X, b.X) && Same(a.Y, b.Y) && Same(a.Z, b.Z) && Same(a.W, b.W);
    }

    inline bool Same(const Quat& a, const Quat& b)
    {
        return Same(a.W, b.W) && Same(a.X, b.X) && Same(a.Y, b.Y) && Same(a.Z, b.Z);
    }

    inline bool Same(const Mat4& a, const Mat4& b)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!Same(a.Rows[i], b.Rows[i]))
                return false;
        }
        return true;
    }

    inline bool Same(const std::vector<Vec2>& a, const std::vector<Vec2>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (!Same(a[i], b[i]))
                return false;
        }
        return true;
    }

    inline bool Same(const Transform& a, const Transform& b)
    {
        return a.Parent == b.Parent && a.Children == b.Children &&
               Same(a.LocalPosition, b.LocalPosition) && Same(a.LocalScale, b.LocalScale) &&
               Same(a.LocalRotation, b.LocalRotation) && Same(a.Affine, b.Affine) &&
               Same(a.Inverse, b.Inverse) && a.Plane == b.Plane && a.IsDirty == b.IsDirty;
    }

    inline bool Same(const AABB& a, const AABB& b)
    {
        return Same(a.OriginalMax, b.OriginalMax) && Same(a.OriginalMin, b.OriginalMin) &&
               Same(a.Max, b.Max) && Same(a.Min, b.Min);
    }

    inline bool Same(const Shape& a, const Shape& b)
    {
        return a.GetShapeType() == b.GetShapeType() && Same(a.Width, b.Width) &&
               Same(a.Height, b.Height) && Same(a.Radius, b.Radius) &&
               Same(a.PolygonPoints, b.PolygonPoints) && Same(a.EdgeNormals, b.EdgeNormals) &&
               Same(a.LocalSpacePoints, b.LocalSpacePoints) && Same(a.Max, b.Max) &&
               Same(a.Min, b.Min);
    }

    inline bool Same(const RigidBody& a, const RigidBody& b)
    {
        return a.IsIntersecting == b.IsIntersecting && a.Initialized == b.Initialized &&
               a.Collidable == b.Collidable && a.Category == b.Category && Same(a.Color, b.Color) &&
               Same(a.RigidBodyAABB, b.RigidBodyAABB) && Same(a.Shape, b.Shape) &&
               Same(a.Position, b.Position) && Same(a.Velocity, b.Velocity) &&
               Same(a.Force, b.Force) && Same(a.Angular, b.Angular) &&
               Same(a.AngularVelocity, b.AngularVelocity) && Same(a.AngularDelta, b.AngularDelta) &&
               Same(a.StaticFriction, b.StaticFriction) &&
               Same(a.DynamicFriction, b.DynamicFriction) && Same(a.InvMass(), b.InvMass()) &&
               Same(a.InvInertia(), b.InvInertia()) && Same(a.Restitution(), b.Restitution());
    }

    // Runtime handles (Loaded / Initialized / shader instance ids) are deliberately
    // not part of the comparison: they are reset on load, see dedicated tests

    inline bool Same(const Mesh& a, const Mesh& b) { return a.MeshType == b.MeshType; }

    inline bool Same(const FragShaderTag& a, const FragShaderTag& b)
    {
        return a.FragAssetId == b.FragAssetId;
    }

    inline bool Same(const VertShaderTag& a, const VertShaderTag& b)
    {
        return a.VertAssetId == b.VertAssetId;
    }

    inline bool Same(const Particle& a, const Particle& b)
    {
        return Same(a.direction, b.direction) && Same(a.Color, b.Color) &&
               Same(a.duration, b.duration);
    }

    inline bool Same(const Emitter& a, const Emitter& b)
    {
        return a.emitterType == b.emitterType && a.density == b.density &&
               Same(a.duration, b.duration) && Same(a.speed, b.speed) && Same(a.size, b.size) &&
               Same(a.particleTime, b.particleTime) && Same(a.coneAngle, b.coneAngle) &&
               Same(a.direction, b.direction) && Same(a.color, b.color);
    }

    inline bool Same(const UITarget& a, const UITarget& b) { return a.active == b.active; }

    inline bool Same(const AIObstacle& a, const AIObstacle& b)
    {
        return Same(a.Width, b.Width) && Same(a.Height, b.Height);
    }

    inline bool Same(const PlayerControlUnit& a, const PlayerControlUnit& b)
    {
        return a.battalionId == b.battalionId && a.health == b.health && a.selected == b.selected &&
               a.isTank == b.isTank && a.isWall == b.isWall;
    }

    inline bool Same(const BasicEnemyUnit& a, const BasicEnemyUnit& b)
    {
        return a.health == b.health && Same(a.Speed, b.Speed);
    }

    inline bool Same(const CrystalDeposit& a, const CrystalDeposit& b)
    {
        return a.AmountOfCrystal == b.AmountOfCrystal;
    }

    inline bool Same(const PlayerBaseComponent& a, const PlayerBaseComponent& b)
    {
        return a.PlayerBaseHealth == b.PlayerBaseHealth;
    }

    inline bool Same(const TankBullet& a, const TankBullet& b) { return Same(a.fuseTime, b.fuseTime); }

    inline bool Same(const Explosion& a, const Explosion& b)
    {
        return Same(a.Duration, b.Duration) && Same(a.GrowthFactor, b.GrowthFactor);
    }

    inline bool Same(const LaserProjectile& a, const LaserProjectile& b)
    {
        return Same(a.LaserTime, b.LaserTime);
    }

    inline bool Same(const GameState& a, const GameState& b)
    {
        return a.ObstacleInCursor == b.ObstacleInCursor &&
               Same(a.OffscreenPosition, b.OffscreenPosition) &&
               Same(a.PrepPhaseTime, b.PrepPhaseTime) &&
               Same(a.InvasionPhaseTime, b.InvasionPhaseTime) &&
               Same(a.CurrentTimeInvasion, b.CurrentTimeInvasion) &&
               Same(a.CurrentTimePrep, b.CurrentTimePrep) && a.battalionCount == b.battalionCount &&
               a.PlayerCrystalInventory == b.PlayerCrystalInventory &&
               a.RoundNumber == b.RoundNumber && a.currentState == b.currentState &&
               Same(a.enemySpeedUpper, b.enemySpeedUpper) &&
               Same(a.enemySpeedLower, b.enemySpeedLower) && a.enemyHealth == b.enemyHealth &&
               Same(a.spawnDistance, b.spawnDistance) && Same(a.currentInterval, b.currentInterval) &&
               a.SpawnVolume == b.SpawnVolume && Same(a.SpawnInterval, b.SpawnInterval) &&
               Same(a.EnemyToTankRatio, b.EnemyToTankRatio) &&
               Same(a.StartPosition, b.StartPosition) && Same(a.StartCamera, b.StartCamera) &&
               Same(a.StartCameraTarget, b.StartCameraTarget) &&
               a.CurCameraState == b.CurCameraState && a.CameraEndState == b.CameraEndState &&
               Same(a.TransformStart, b.TransformStart) && Same(a.TransformEnd, b.TransformEnd) &&
               Same(a.CameraFollow, b.CameraFollow) && Same(a.LerpProgress, b.LerpProgress) &&
               Same(a.LerpTime, b.LerpTime);
    }

    inline bool SameFieldConfig(const VectorField& a, const VectorField& b)
    {
        return Same(a.LocationVectorField, b.LocationVectorField) &&
               a.GridCountHeight == b.GridCountHeight && a.GridCountWidth == b.GridCountWidth &&
               Same(a.HalfHeight, b.HalfHeight) && Same(a.HalfWidth, b.HalfWidth);
    }

    inline bool Same(const std::unordered_map<Entity, Vec3>& a,
                     const std::unordered_map<Entity, Vec3>& b)
    {
        if (a.size() != b.size())
            return false;
        for (const auto& kv : a)
        {
            auto it = b.find(kv.first);
            if (it == b.end() || !Same(kv.second, it->second))
                return false;
        }
        return true;
    }

    inline bool SameAIMemory(const BlackBoard& a, const BlackBoard& b)
    {
        return a.UnitTarget == b.UnitTarget && a.EnemyTarget == b.EnemyTarget &&
               SameFieldConfig(a.UnitVectorField, b.UnitVectorField) &&
               SameFieldConfig(a.EnemyVectorField, b.EnemyVectorField) &&
               a.PlayerTankTargets == b.PlayerTankTargets &&
               a.EnemyTankTargets == b.EnemyTankTargets &&
               Same(a.LastKnownLocation, b.LastKnownLocation) && a.InLineOfSight == b.InLineOfSight &&
               Same(a.PatrolTargets, b.PatrolTargets) && Same(a.DeltaTime, b.DeltaTime);
    }

    //-----------------------------------------------------------------------------
    // WorldImage: independent copy of the complete ECS state
    //-----------------------------------------------------------------------------

    struct WorldImage
    {
        std::vector<Entity> Living;
        std::vector<Entity> Available;
        std::map<Entity, Transform> Transforms;
        std::map<Entity, RigidBody> RigidBodies;
        std::map<Entity, Mesh> Meshes;
        std::map<Entity, FragShaderTag> FragShaders;
        std::map<Entity, VertShaderTag> VertShaders;
        std::map<Entity, Particle> Particles;
        std::map<Entity, Emitter> Emitters;
        std::map<Entity, UITarget> UITargets;
        std::map<Entity, AIObstacle> Obstacles;
        std::map<Entity, PlayerControlUnit> PlayerUnits;
        std::map<Entity, BasicEnemyUnit> EnemyUnits;
        std::map<Entity, CrystalDeposit> Crystals;
        std::map<Entity, PlayerBaseComponent> Bases;
        std::map<Entity, TankBullet> Bullets;
        std::map<Entity, Explosion> Explosions;
        std::map<Entity, LaserProjectile> Lasers;
        GameState State;
        BlackBoard Board;
        UIContextState UIMode = DefaultContext;
        bool UIFlipped = false;
    };

    template <typename T>
    void CaptureComponent(ECSManager& ecs, const std::vector<Entity>& living, std::map<Entity, T>& out)
    {
        for (Entity e : living)
        {
            if (ecs.HasComponent<T>(e))
                out.emplace(e, ecs.GetComponent<T>(e));
        }
    }

    inline WorldImage Capture(ECSManager& ecs)
    {
        WorldImage image;
        image.Living = ecs.GetLivingEntities();
        image.Available = ecs.GetAvailableEntities();
        const auto& l = image.Living;
        CaptureComponent(ecs, l, image.Transforms);
        CaptureComponent(ecs, l, image.RigidBodies);
        CaptureComponent(ecs, l, image.Meshes);
        CaptureComponent(ecs, l, image.FragShaders);
        CaptureComponent(ecs, l, image.VertShaders);
        CaptureComponent(ecs, l, image.Particles);
        CaptureComponent(ecs, l, image.Emitters);
        CaptureComponent(ecs, l, image.UITargets);
        CaptureComponent(ecs, l, image.Obstacles);
        CaptureComponent(ecs, l, image.PlayerUnits);
        CaptureComponent(ecs, l, image.EnemyUnits);
        CaptureComponent(ecs, l, image.Crystals);
        CaptureComponent(ecs, l, image.Bases);
        CaptureComponent(ecs, l, image.Bullets);
        CaptureComponent(ecs, l, image.Explosions);
        CaptureComponent(ecs, l, image.Lasers);
        image.State = *ecs.GetResource<GameState>();
        const BlackBoard& board = *ecs.GetResource<BlackBoard>();
        image.Board.UnitTarget = board.UnitTarget;
        image.Board.EnemyTarget = board.EnemyTarget;
        image.Board.UnitVectorField = board.UnitVectorField;
        image.Board.EnemyVectorField = board.EnemyVectorField;
        image.Board.PlayerTankTargets = board.PlayerTankTargets;
        image.Board.EnemyTankTargets = board.EnemyTankTargets;
        image.Board.LastKnownLocation = board.LastKnownLocation;
        image.Board.InLineOfSight = board.InLineOfSight;
        image.Board.PatrolTargets = board.PatrolTargets;
        image.Board.DeltaTime = board.DeltaTime;
        image.UIMode = ecs.GetResource<UIState>()->state;
        image.UIFlipped = ecs.GetResource<UIState>()->flipped;
        return image;
    }

    template <typename T>
    void DiffComponent(const char* name,
                       const std::map<Entity, T>& a,
                       const std::map<Entity, T>& b,
                       std::vector<std::string>& diffs)
    {
        if (a.size() != b.size())
        {
            diffs.push_back(std::string(name) + ": count " + std::to_string(a.size()) + " vs " +
                            std::to_string(b.size()));
        }
        for (const auto& kv : a)
        {
            auto it = b.find(kv.first);
            if (it == b.end())
                diffs.push_back(std::string(name) + ": missing on entity " + std::to_string(kv.first));
            else if (!Same(kv.second, it->second))
                diffs.push_back(std::string(name) + ": differs on entity " + std::to_string(kv.first));
        }
    }

    /**
     * \brief List every difference between two world images (empty = identical)
     */
    inline std::vector<std::string> Diff(const WorldImage& a, const WorldImage& b)
    {
        std::vector<std::string> diffs;
        if (a.Living != b.Living)
            diffs.push_back("living entity list differs");
        if (a.Available != b.Available)
            diffs.push_back("free entity queue differs");
        DiffComponent("Transform", a.Transforms, b.Transforms, diffs);
        DiffComponent("RigidBody", a.RigidBodies, b.RigidBodies, diffs);
        DiffComponent("Mesh", a.Meshes, b.Meshes, diffs);
        DiffComponent("FragShaderTag", a.FragShaders, b.FragShaders, diffs);
        DiffComponent("VertShaderTag", a.VertShaders, b.VertShaders, diffs);
        DiffComponent("Particle", a.Particles, b.Particles, diffs);
        DiffComponent("Emitter", a.Emitters, b.Emitters, diffs);
        DiffComponent("UITarget", a.UITargets, b.UITargets, diffs);
        DiffComponent("AIObstacle", a.Obstacles, b.Obstacles, diffs);
        DiffComponent("PlayerControlUnit", a.PlayerUnits, b.PlayerUnits, diffs);
        DiffComponent("BasicEnemyUnit", a.EnemyUnits, b.EnemyUnits, diffs);
        DiffComponent("CrystalDeposit", a.Crystals, b.Crystals, diffs);
        DiffComponent("PlayerBase", a.Bases, b.Bases, diffs);
        DiffComponent("TankBullet", a.Bullets, b.Bullets, diffs);
        DiffComponent("Explosion", a.Explosions, b.Explosions, diffs);
        DiffComponent("LaserProjectile", a.Lasers, b.Lasers, diffs);
        if (!Same(a.State, b.State))
            diffs.push_back("GameState differs");
        if (!SameAIMemory(a.Board, b.Board))
            diffs.push_back("BlackBoard AI memory differs");
        if (a.UIMode != b.UIMode || a.UIFlipped != b.UIFlipped)
            diffs.push_back("UIState interaction mode differs");
        return diffs;
    }

    inline std::string Describe(const std::vector<std::string>& diffs)
    {
        std::ostringstream out;
        for (const auto& d : diffs)
            out << "\n        - " << d;
        return out.str();
    }

#define CHECK_SAME_WORLD(a, b)                                                                 \
    do                                                                                         \
    {                                                                                          \
        ++TestFramework::TotalChecks();                                                        \
        auto tfDiffs = Fixture::Diff((a), (b));                                                \
        if (!tfDiffs.empty())                                                                  \
            TestFramework::ReportFailure(__FILE__, __LINE__,                                   \
                                         "worlds differ:" + Fixture::Describe(tfDiffs));       \
    } while (0)

    //-----------------------------------------------------------------------------
    // World setup
    //-----------------------------------------------------------------------------

    /**
     * \brief Registers the resources the save system uses (once per process) and
     *        clears the world
     */
    inline void FreshWorld()
    {
        static bool registered = false;
        if (!registered)
        {
            ECS.RegisterResource(GameState());
            ECS.RegisterResource(BlackBoard());
            ECS.RegisterResource(UIState());
            registered = true;
        }
        ECS.Reset();
        // UIState::ResetResource does nothing (same as in the game), reset it here
        // so every test starts from the same interaction mode
        auto ui = ECS.GetResource<UIState>();
        ui->state = DefaultContext;
        ui->flipped = false;
    }

    /**
     * \brief Same as the game's CreateMeshEntity (GameUtils.cpp) without the
     *        asset loading dependencies
     */
    inline Entity MeshEntity(Vec3 position, ObjAsset asset, Quat rotate = Quat(), Vec3 scale = {1, 1, 1})
    {
        Entity e = ECS.CreateEntity();
        Transform t = Transform(position);
        t.SetGlobalRotation(rotate);
        t.Scale(scale);
        t.Plane = XZ;
        ECS.AddComponent<Transform>(e, t);
        Mesh mesh(asset);
        // Pretend the render system already processed the entity
        mesh.Loaded = true;
        ECS.AddComponent<Mesh>(e, mesh);
        return e;
    }

    inline FragShaderTag RunningShader(FragShaderTypeID id, size_t instance)
    {
        FragShaderTag tag(id);
        // Pretend the ShaderHandler already created an instance for the entity
        tag.FragShaderID = instance;
        tag.Initialized = true;
        return tag;
    }

    /**
     * \brief Notable entities of the sample level
     */
    struct SampleLevel
    {
        Entity Ground = NULL_ENTITY;
        Entity Base = NULL_ENTITY;
        Entity Selector = NULL_ENTITY;
        std::vector<Entity> Soldiers;
        std::vector<Entity> Supports;
        Entity PlayerTank = NULL_ENTITY;
        Entity PlayerTankBase = NULL_ENTITY;
        Entity PlayerTankCannon = NULL_ENTITY;
        std::vector<Entity> Enemies;
        Entity EnemyTank = NULL_ENTITY;
        Entity EnemyTankCannon = NULL_ENTITY;
        std::vector<Entity> Crystals;
        Entity Bullet = NULL_ENTITY;
        Entity BulletEmitter = NULL_ENTITY;
        Entity Explosion = NULL_ENTITY;
        Entity Laser = NULL_ENTITY;
        Entity Particle = NULL_ENTITY;
        Entity WallInCursor = NULL_ENTITY;
        std::vector<Entity> Destroyed;
    };

    /**
     * \brief Build a Metal Invasion level in the middle of round 4's invasion
     *        phase (mirrors CreateMainLevel / PlayerUnits / BasicEnemyUnit ...)
     */
    inline SampleLevel BuildSampleLevel()
    {
        FreshWorld();
        SampleLevel level;
        size_t shaderInstance = 1;

        // -- Level -----------------------------------------------------------
        level.Ground = MeshEntity({0, 0, 0}, Ground);

        level.Base = MeshEntity({0, 0, 0}, PlayerBase);
        RigidBody baseBody(2.5f, 2.5f);
        baseBody.SetStatic();
        baseBody.Category = UnitCollider;
        baseBody.Initialized = true;
        ECS.AddComponent<RigidBody>(level.Base, baseBody);
        ECS.AddComponent<PlayerBaseComponent>(level.Base, {730});
        ECS.AddComponent<FragShaderTag>(level.Base, RunningShader(BlinnPhongID, shaderInstance++));
        ECS.AddComponent<AIObstacle>(level.Base, {3, 3});

        level.Selector = MeshEntity({2, 2, 0}, TargetUISelector);
        ECS.AddComponent<UITarget>(level.Selector, {true});

        // -- Player battalions -------------------------------------------------
        for (int i = 0; i < 4; ++i)
        {
            Entity soldier = MeshEntity({1.5f + 0.5f * i, 0, -3.25f}, SoldierUnitAsset);
            RigidBody body(0.30f, 0.30f);
            body.Category = UnitCollider;
            body.Initialized = true;
            body.Velocity = Vec2(0.01f * (i + 1), -0.0035f);
            body.Angular = 0.3f * i;
            body.AngularVelocity = 0.001f;
            body.Shape.RecomputePoints(body.Angular, Vec2(1.5f + 0.5f * i, -3.25f));
            ECS.AddComponent<PlayerControlUnit>(soldier, {1, 100 - 15 * i, i % 2 == 0});
            ECS.AddComponent<RigidBody>(soldier, body);
            ECS.AddComponent<FragShaderTag>(soldier, RunningShader(BlinnPhongID, shaderInstance++));
            level.Soldiers.push_back(soldier);
        }
        for (int i = 0; i < 2; ++i)
        {
            Entity support = MeshEntity({-4.0f, 0, 2.0f + 0.5f * i}, SupportUnitAsset);
            RigidBody body(0.30f, 0.30f);
            body.Category = UnitCollider;
            ECS.AddComponent<PlayerControlUnit>(support, {2, 60, false});
            ECS.AddComponent<RigidBody>(support, body);
            ECS.AddComponent<FragShaderTag>(support, RunningShader(BlinnPhongID, shaderInstance++));
            level.Supports.push_back(support);
        }

        // -- Player tank (root + two child meshes) ---------------------------
        level.PlayerTank = ECS.CreateEntity();
        Transform tankT(Vec3(5.0f, 0.0f, 5.0f), Quat(Vec3(0, 1, 0), 0.75f));
        tankT.Plane = XZ;
        ECS.AddComponent<Transform>(level.PlayerTank, tankT);
        RigidBody tankBody(0.5f, 0.5f);
        tankBody.Category = UnitCollider;
        ECS.AddComponent<RigidBody>(level.PlayerTank, tankBody);
        ECS.AddComponent<PlayerControlUnit>(level.PlayerTank, {3, 180, true, true});
        ECS.AddComponent<FragShaderTag>(level.PlayerTank, RunningShader(BlinnPhongID, shaderInstance++));
        level.PlayerTankBase = MeshEntity({0, 0, 0}, BaseTank, Quat(), {3, 3, 3});
        ECS.GetComponent<Transform>(level.PlayerTankBase).SetParentEntity(level.PlayerTank, level.PlayerTankBase);
        level.PlayerTankCannon = MeshEntity({0, 0, 0}, CannonTank, Quat(Vec3(0, 1, 0), 1.2f), {3, 3, 3});
        ECS.GetComponent<Transform>(level.PlayerTankCannon)
                .SetParentEntity(level.PlayerTank, level.PlayerTankCannon);

        // -- Enemies ----------------------------------------------------------
        for (int i = 0; i < 4; ++i)
        {
            Entity enemy = MeshEntity({18.0f - i, 0, 12.0f + 0.5f * i}, BasicEnemy);
            RigidBody body(0.30f, 0.30f);
            body.Category = UnitCollider;
            body.Velocity = Vec2(-0.002f, -0.001f * i);
            ECS.AddComponent<BasicEnemyUnit>(enemy, {100 - 20 * i, 0.001f + 0.0001f * i});
            ECS.AddComponent<RigidBody>(enemy, body);
            ECS.AddComponent<FragShaderTag>(enemy, RunningShader(BlinnPhongID, shaderInstance++));
            level.Enemies.push_back(enemy);
        }

        level.EnemyTank = ECS.CreateEntity();
        Transform enemyTankT(Vec3(-15.0f, 0.0f, 14.0f), Quat());
        enemyTankT.Plane = XZ;
        ECS.AddComponent<Transform>(level.EnemyTank, enemyTankT);
        RigidBody enemyTankBody(0.5f, 0.5f);
        enemyTankBody.Category = UnitCollider;
        ECS.AddComponent<RigidBody>(level.EnemyTank, enemyTankBody);
        ECS.AddComponent<BasicEnemyUnit>(level.EnemyTank, {200});
        ECS.AddComponent<FragShaderTag>(level.EnemyTank, RunningShader(BlinnPhongID, shaderInstance++));
        Entity enemyTankBase = MeshEntity({0, 0, 0}, BaseTank, Quat(), {3, 3, 3});
        ECS.GetComponent<Transform>(enemyTankBase).SetParentEntity(level.EnemyTank, enemyTankBase);
        level.EnemyTankCannon = MeshEntity({0, 0, 0}, CannonTank, Quat(), {3, 3, 3});
        ECS.GetComponent<Transform>(level.EnemyTankCannon)
                .SetParentEntity(level.EnemyTank, level.EnemyTankCannon);

        // -- Crystals ---------------------------------------------------------
        const int crystalAmounts[] = {30, 12, 1};
        for (int i = 0; i < 3; ++i)
        {
            Entity crystal = MeshEntity({-10.0f + 4.0f * i, 0, -12.0f}, CrystalAsset);
            ECS.AddComponent<CrystalDeposit>(crystal, {crystalAmounts[i]});
            ECS.AddComponent<AIObstacle>(crystal, {0.5f, 0.5f});
            level.Crystals.push_back(crystal);
        }

        // -- Projectiles and effects --------------------------------------------
        Quat bulletRotation(Vec3(0, 1, 0), 0.4f);
        level.Bullet = MeshEntity({6.0f, 0, 6.0f}, Bullet, bulletRotation, {3, 3, 3});
        RigidBody bulletBody(0.15f, 0.15f);
        bulletBody.Category = BulletCollider;
        bulletBody.DynamicFriction = 0.0f;
        bulletBody.ApplyImpulse(Vec2(0.19f, 0.46f) * 0.5f);
        ECS.AddComponent<RigidBody>(level.Bullet, bulletBody);
        ECS.AddComponent<TankBullet>(level.Bullet, {1.25f});

        level.BulletEmitter = ECS.CreateEntity();
        Emitter emit;
        emit.emitterType = Cone;
        emit.density = 5;
        emit.direction = Vec3(0.19f, 0.0f, 0.46f);
        emit.speed = 0.5f;
        emit.size = 0.2f;
        emit.particleTime = 1.5f;
        emit.duration = 0.2f;
        emit.color = Vec3(1.0f, 0.0f, 0.0f);
        emit.coneAngle = 60.0f;
        ECS.AddComponent<Transform>(level.BulletEmitter, Transform(Vec3(6.2f, 0.0f, 6.4f)));
        ECS.AddComponent<Emitter>(level.BulletEmitter, emit);

        level.Explosion = MeshEntity({12.0f, 0, 9.0f}, ExplosionBall);
        RigidBody explosionBody(0.5f);
        explosionBody.Collidable = false;
        explosionBody.Category = ExplosionCollider;
        explosionBody.UpdateRadius(0.62f);
        ECS.AddComponent<FragShaderTag>(level.Explosion, RunningShader(OutlineShaderID, shaderInstance++));
        ECS.AddComponent<RigidBody>(level.Explosion, explosionBody);
        ECS.AddComponent<Explosion>(level.Explosion, {0.8f, 0.25f});

        Quat laserRotation(Vec3(0, 1, 0), 2.1f);
        level.Laser = MeshEntity({2.0f, 0, -3.0f}, Laser, laserRotation, {1, 1, 4.5f});
        ECS.AddComponent<LaserProjectile>(level.Laser, {0.12f});

        level.Particle = ECS.CreateEntity();
        ECS.AddComponent<Transform>(level.Particle, Transform(Vec3(6.3f, 0.4f, 6.5f)));
        ECS.AddComponent<::Particle>(level.Particle, {Vec3(0.1f, 0.5f, 0.2f), Vec3(1, 0.2f, 0), true, 0.7f});
        ECS.AddComponent<FragShaderTag>(level.Particle, RunningShader(ParticleShaderID, shaderInstance++));
        VertShaderTag vertTag(DefaultVertShaderID);
        vertTag.Initialized = true;
        vertTag.VertShaderID = 3;
        ECS.AddComponent<VertShaderTag>(level.Particle, vertTag);

        // -- Obstacle currently being placed by the player ----------------------
        level.WallInCursor = MeshEntity({-2.0f, 0, 7.0f}, ObstacleWall);
        RigidBody wallBody(1.0f, 10.0f);
        wallBody.Category = UnitCollider;
        wallBody.IsIntersecting = true;
        ECS.AddComponent<RigidBody>(level.WallInCursor, wallBody);
        ECS.AddComponent<PlayerControlUnit>(level.WallInCursor, {-1, 250, false});
        ECS.AddComponent<FragShaderTag>(level.WallInCursor, RunningShader(RedShaderID, shaderInstance++));

        // -- Units which died this round (holes in the entity id space) ---------
        level.Destroyed.push_back(level.Enemies[1]);
        level.Destroyed.push_back(level.Soldiers[3]);
        ECS.DestroyEntity(level.Enemies[1]);
        ECS.DestroyEntity(level.Soldiers[3]);
        level.Enemies.erase(level.Enemies.begin() + 1);
        level.Soldiers.pop_back();
        ECS.FlushECS();

        // -- Round state ------------------------------------------------------
        auto state = ECS.GetResource<GameState>();
        state->ObstacleInCursor = level.WallInCursor;
        state->RoundNumber = 4;
        state->currentState = Invasion;
        state->PlayerCrystalInventory = 42;
        state->SpawnVolume = 7;
        state->InvasionPhaseTime = 65.0f;
        state->CurrentTimeInvasion = 13.37f;
        state->CurrentTimePrep = 0.0f;
        state->currentInterval = 4.5f;
        state->battalionCount = 3;
        state->enemyHealth = 140;
        state->StartPosition = Vec3(0, 10, -5);
        state->StartCamera = Vec3(0, 10, -5);
        state->StartCameraTarget = Vec3(0, 0, 0);
        state->CurCameraState = LerpToPosition;
        state->CameraEndState = StartMenu;
        state->TransformStart = Transform(Vec3(0, 10, -5));
        state->TransformEnd = Transform(Vec3(1, 8, -4), Quat(Vec3(1, 0, 0), 0.5f));
        state->CameraFollow = Vec3(0.5f, 0, 0.25f);
        state->LerpProgress = 0.35f;
        state->LerpTime = 2.0f;

        // The player is in build mode (wall already paid for) and rotated the wall
        auto ui = ECS.GetResource<UIState>();
        ui->state = BuildObstacleContext;
        ui->flipped = true;
        ui->mouseX = 512.0f;

        // -- AI memory -------------------------------------------------------------
        auto board = ECS.GetResource<BlackBoard>();
        board->UnitTarget = level.Selector;
        board->EnemyTarget = level.Base;
        board->UnitVectorField.HalfWidth = 25.0f;
        board->UnitVectorField.HalfHeight = 25.0f;
        board->UnitVectorField.LocationVectorField = Vec3(0, 0, 0);
        board->EnemyVectorField.HalfWidth = 25.0f;
        board->EnemyVectorField.HalfHeight = 25.0f;
        board->EnemyVectorField.GridCountWidth = 40;
        board->PlayerTankTargets[level.PlayerTankCannon] = level.Enemies[0];
        board->EnemyTankTargets[level.EnemyTankCannon] = level.Soldiers[0];
        board->LastKnownLocation[level.Enemies[0]] = Vec3(17.0f, 0.0f, 12.0f);
        board->InLineOfSight[level.Enemies[0]] = true;
        board->InLineOfSight[level.Enemies[2]] = false;
        board->PatrolTargets[level.EnemyTank] = Vec3(-10, 0, 10);
        board->DeltaTime = 16.6667f;

        return level;
    }

    inline const Serialization::SerializationRegistry& Registry()
    {
        return Serialization::GetGameSerializationRegistry();
    }
} // namespace Fixture
