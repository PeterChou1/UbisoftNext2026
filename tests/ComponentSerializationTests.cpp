//---------------------------------------------------------------------------------
// ComponentSerializationTests.cpp
//---------------------------------------------------------------------------------
//
// Round trip tests of individual engine / game components and resources
//
#include "GameWorldFixture.h"

using namespace Serialization;
using Fixture::Same;

namespace
{
    template <typename T>
    T RoundTrip(T& value)
    {
        T loaded{};
        FromBytes(ToBytes(value), loaded);
        return loaded;
    }
} // namespace

TEST_CASE("Component: Transform keeps hierarchy, TRS and matrices")
{
    Transform t(Vec3(1.5f, -2.0f, 3.25f), Quat(Vec3(0, 1, 0), 0.6f), Vec3(2, 3, 4));
    t.Parent = 17;
    t.Children = {18, 19, 250};
    t.Plane = XZ;
    t.IsDirty = true;

    Transform loaded = RoundTrip(t);
    CHECK(Same(loaded, t));
    CHECK_EQ(loaded.Parent, Entity(17));
    CHECK(loaded.Children == std::vector<Entity>({18, 19, 250}));
    CHECK(loaded.Plane == XZ);
}

TEST_CASE("Component: RigidBody keeps private mass / inertia / restitution")
{
    RigidBody body(0.5f, 1.5f, 2.0f);
    body.Category = BulletCollider;
    body.Collidable = false;
    body.Velocity = Vec2(0.25f, -0.125f);
    body.Force = Vec2(1, 2);
    body.AngularVelocity = 0.01f;
    body.Shape.RecomputePoints(0.3f, Vec2(4, 5));
    body.RecomputeAABB();

    RigidBody loaded = RoundTrip(body);
    CHECK(Same(loaded, body));
    CHECK(loaded.InvMass() > 0.0f);
    CHECK(Same(loaded.InvMass(), body.InvMass()));
    CHECK(Same(loaded.InvInertia(), body.InvInertia()));
    CHECK(Same(loaded.Restitution(), body.Restitution()));
    CHECK(loaded.Shape.GetShapeType() == PolygonShape);
    CHECK_EQ(loaded.Shape.PolygonPoints.size(), size_t(4));
}

TEST_CASE("Component: static and circular RigidBody")
{
    RigidBody wall(1.0f, 10.0f);
    wall.SetStatic();
    RigidBody loadedWall = RoundTrip(wall);
    CHECK(Same(loadedWall, wall));
    CHECK_EQ(loadedWall.InvMass(), 0.0f);
    CHECK_EQ(loadedWall.InvInertia(), 0.0f);

    RigidBody ball(0.75f);
    RigidBody loadedBall = RoundTrip(ball);
    CHECK(Same(loadedBall, ball));
    CHECK(loadedBall.Shape.GetShapeType() == CircleShape);
    CHECK(Same(loadedBall.Shape.Radius, 0.75f));
}

TEST_CASE("Component: physics scratch buffers are not persisted")
{
    RigidBody body(1.0f, 1.0f);
    body.Shape.ContactPoints = {Vec2(1, 1)};
    body.Shape.DebugPoints = {Vec2(2, 2), Vec2(3, 3)};
    RigidBody loaded = RoundTrip(body);
    CHECK(loaded.Shape.ContactPoints.empty());
    CHECK(loaded.Shape.DebugPoints.empty());
}

TEST_CASE("Component: render handles are reset so the render systems rebuild them")
{
    // Load into objects whose handles are "live" to prove the load resets them
    Mesh mesh(CannonTank);
    mesh.Loaded = true;
    Mesh loadedMesh(Ground);
    loadedMesh.Loaded = true;
    FromBytes(ToBytes(mesh), loadedMesh);
    CHECK(loadedMesh.MeshType == CannonTank);
    CHECK_EQ(loadedMesh.Loaded, false);

    FragShaderTag frag(ToonShaderID);
    frag.FragShaderID = 12;
    frag.Initialized = true;
    FragShaderTag loadedFrag = frag;
    FromBytes(ToBytes(frag), loadedFrag);
    CHECK(loadedFrag.FragAssetId == ToonShaderID);
    CHECK_EQ(loadedFrag.FragShaderID, size_t(0));
    CHECK_EQ(loadedFrag.Initialized, false);

    VertShaderTag vert(DefaultVertShaderID);
    vert.VertShaderID = 4;
    vert.Initialized = true;
    VertShaderTag loadedVert = vert;
    FromBytes(ToBytes(vert), loadedVert);
    CHECK_EQ(loadedVert.VertShaderID, size_t(0));
    CHECK_EQ(loadedVert.Initialized, false);

    Particle particle{Vec3(1, 2, 3), Vec3(0.5f, 0.5f, 0), true, 0.4f};
    Particle loadedParticle = particle;
    FromBytes(ToBytes(particle), loadedParticle);
    CHECK(Same(loadedParticle, particle));
    CHECK_EQ(loadedParticle.loaded, false);

    // Handles are not written at all: saves of live and fresh objects are identical
    Mesh fresh(CannonTank);
    CHECK(ToBytes(mesh) == ToBytes(fresh));
}

TEST_CASE("Component: gameplay components round trip")
{
    PlayerControlUnit unit{7, 55, true, true, false};
    CHECK(Same(RoundTrip(unit), unit));

    BasicEnemyUnit enemy{80, 0.00125f};
    CHECK(Same(RoundTrip(enemy), enemy));

    CrystalDeposit crystal{17};
    CHECK(Same(RoundTrip(crystal), crystal));

    PlayerBaseComponent base{321};
    CHECK(Same(RoundTrip(base), base));

    TankBullet bullet{0.75f};
    CHECK(Same(RoundTrip(bullet), bullet));

    Explosion explosion{1.1f, 0.3f};
    CHECK(Same(RoundTrip(explosion), explosion));

    LaserProjectile laser{0.05f};
    CHECK(Same(RoundTrip(laser), laser));

    UITarget target{true};
    CHECK(Same(RoundTrip(target), target));

    AIObstacle obstacle{5, 1};
    CHECK(Same(RoundTrip(obstacle), obstacle));

    Emitter emitter;
    emitter.emitterType = Cone;
    emitter.density = 9;
    emitter.coneAngle = 45.0f;
    emitter.direction = Vec3(0, 1, 0);
    emitter.color = Vec3(1, 0.5f, 0);
    CHECK(Same(RoundTrip(emitter), emitter));
}

TEST_CASE("Resource: GameState keeps round flow, economy, difficulty and camera state")
{
    GameState state;
    state.ObstacleInCursor = 99;
    state.RoundNumber = 12;
    state.currentState = Prepartion;
    state.PlayerCrystalInventory = 1234;
    state.SpawnVolume = 25;
    state.CurrentTimePrep = 42.5f;
    state.CurrentTimeInvasion = 0.0f;
    state.InvasionPhaseTime = 200.0f;
    state.enemyHealth = 400;
    state.enemySpeedUpper = 0.004f;
    state.CurCameraState = OptionsMenu;
    state.CameraEndState = LerpToPosition;
    state.TransformStart = Transform(Vec3(1, 2, 3));
    state.TransformEnd = Transform(Vec3(4, 5, 6), Quat(Vec3(0, 0, 1), 0.2f));
    state.CameraFollow = Vec3(9, 8, 7);
    state.LerpProgress = 0.5f;
    state.LerpTime = 3.0f;

    GameState loaded;
    FromBytes(ToBytes(state), loaded);
    CHECK(Same(loaded, state));
    CHECK_EQ(loaded.RoundNumber, 12);
    CHECK(loaded.currentState == Prepartion);
}

TEST_CASE("Resource: BlackBoard keeps AI memory but not behaviour trees or grids")
{
    BlackBoard board;
    board.UnitTarget = 5;
    board.EnemyTarget = 2;
    board.UnitVectorField.HalfWidth = 30.0f;
    board.UnitVectorField.GridCountHeight = 80;
    board.PlayerTankTargets = {{10, 20}, {11, 21}};
    board.EnemyTankTargets = {{30, 3}};
    board.LastKnownLocation = {{20, Vec3(1, 0, 1)}};
    board.InLineOfSight = {{20, true}, {21, false}};
    board.PatrolTargets = {{30, Vec3(-1, 0, 4)}};
    board.DeltaTime = 16.0f;
    Vec3 origin(0, 0, 0);
    board.UnitVectorField.CreateVectorField(origin);
    board.BehaviorTreeDataBase[10] = nullptr;

    BlackBoard loaded;
    loaded.UnitTarget = NULL_ENTITY;
    loaded.EnemyTarget = NULL_ENTITY;
    FromBytes(ToBytes(board), loaded);
    CHECK(Fixture::SameAIMemory(loaded, board));
    // Derived / runtime data is rebuilt by the game after loading
    CHECK(loaded.UnitVectorField.Map.empty());
    CHECK(loaded.BehaviorTreeDataBase.empty());
}

TEST_CASE("Resource: UIState keeps the interaction mode but not per frame input")
{
    UIState ui;
    ui.state = BuildObstacleContext;
    ui.flipped = true;
    ui.mouseX = 100.0f;
    ui.leftClick = true;
    ui.hotItem = 4;

    UIState loaded;
    loaded.mouseX = 7.0f;
    FromBytes(ToBytes(ui), loaded);
    CHECK(loaded.state == BuildObstacleContext);
    CHECK(loaded.flipped);
    // Input state is left alone, it belongs to the current frame
    CHECK(Same(loaded.mouseX, 7.0f));
    CHECK(!loaded.leftClick);
    CHECK_EQ(loaded.hotItem, -1);
}
