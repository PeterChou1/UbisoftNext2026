//---------------------------------------------------------------------------------
// PhysicsTests.cpp
//---------------------------------------------------------------------------------
//
// The physics system's optimisations must not change the simulation:
//
//   - every broad phase (all pairs, sort and sweep, uniform grid) finds the
//     same pairs, in the same order: bit identical results
//   - transforms written once per step (no callbacks) end exactly where the
//     per sub step writes (with a category callback registered) end
//   - fewer pairs are tested, and static bodies are left alone
//
// Timing is measured by the PhysicsBenchmark tool (tests/tools)
//
#include "ColliderCallbackSystem.h"
#include "PhysicsSystem.h"
#include "WorldFixture.h"

#include <cmath>
#include <random>

using SceneObjects::BodyType;

namespace
{
    Entity Body(Shape2DType type, const Vec3& at, float w, float h, BodyType body, float yaw = 0.0f)
    {
        SceneObjects::ShapeDesc desc;
        desc.Shape.Type = type;
        desc.Shape.Width = w;
        desc.Shape.Height = h;
        desc.Shape.Sides = 6;
        desc.Position = at;
        desc.YawDegrees = yaw;
        desc.Body = body;
        return SceneObjects::CreateShape(desc);
    }

    // A walled arena with a dense pile, bodies flying around, a long wall
    // (a "big" body for the grid), a trigger and a parent / child pair
    void BuildScene(int moving)
    {
        Fixture::FreshWorld();
        std::mt19937 random(99);
        std::uniform_real_distribution<float> spot(-11.0f, 11.0f);
        std::uniform_real_distribution<float> speed(-8.0f, 8.0f);
        std::uniform_real_distribution<float> turn(0.0f, 360.0f);
        const float half = 13.0f;
        Body(Shape2DType::Rectangle, {0, 0, half}, 28, 1, BodyType::Static);
        Body(Shape2DType::Rectangle, {0, 0, -half}, 28, 1, BodyType::Static);
        Body(Shape2DType::Rectangle, {half, 0, 0}, 1, 28, BodyType::Static);
        Body(Shape2DType::Rectangle, {-half, 0, 0}, 1, 28, BodyType::Static);
        Body(Shape2DType::Rectangle, {0, 0, 4}, 20, 0.5f, BodyType::Static, 10.0f);
        Body(Shape2DType::Circle, {5, 0, -5}, 3, 3, BodyType::Trigger);
        const Shape2DType types[] = {Shape2DType::Circle, Shape2DType::Rectangle, Shape2DType::Polygon};
        for (int i = 0; i < moving; ++i)
        {
            // Half in a tight pile, half scattered
            Vec3 at = i % 2 == 0 ? Vec3(-8.0f + 0.8f * static_cast<float>((i / 2) % 12), 0.0f,
                                         -10.0f + 0.8f * static_cast<float>((i / 2) / 12))
                                 : Vec3(spot(random), 0.0f, spot(random));
            Entity e = Body(types[i % 3], at, 0.7f, 0.7f, BodyType::Dynamic, turn(random));
            ECS.GetComponent<RigidBody>(e).Velocity = Vec2(speed(random), speed(random));
        }
        // A dynamic child following its dynamic parent
        Entity parent = Body(Shape2DType::Rectangle, {-4, 0, 8}, 1, 1, BodyType::Dynamic, 30.0f);
        Entity child = Body(Shape2DType::Circle, {-3, 0, 8}, 0.5f, 0.5f, BodyType::Dynamic);
        SceneObjects::SetParent(child, parent);
        ECS.GetComponent<RigidBody>(parent).Velocity = Vec2(3.0f, 1.0f);
        ECS.FlushECS();
    }

    struct BodyState
    {
        Entity E;
        float X, Y, Angle, VX, VY, Spin;
        Vec3 World;
        Quat Rotation;
    };

    std::vector<BodyState> Simulate(PhysicsSystem::BroadPhase mode, int moving, int frames, bool callback,
                                    PhysicsSystem::Stats* stats = nullptr)
    {
        BuildScene(moving);
        auto callbacks = ECS.GetResource<ColliderCallbackSystem>();
        if (callback)
            callbacks->RegisterCallback(std::make_shared<Collider>(Category7, Category8));
        PhysicsSystem physics;
        physics.SetBroadPhase(mode);
        for (int i = 0; i < frames; ++i)
            physics.Update(16.5f);
        if (stats != nullptr)
            *stats = physics.GetStats();
        std::vector<BodyState> states;
        for (Entity e : ECS.Visit<RigidBody>())
        {
            const RigidBody& b = ECS.GetComponent<RigidBody>(e);
            Transform& t = ECS.GetComponent<Transform>(e);
            states.push_back({e, b.Position.X, b.Position.Y, b.Angular, b.Velocity.X, b.Velocity.Y, b.AngularVelocity,
                              t.GetWorldPosition(), t.LocalRotation});
        }
        callbacks->ResetResource();
        return states;
    }

    bool Same(float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); }

    bool Identical(const std::vector<BodyState>& a, const std::vector<BodyState>& b, bool transforms)
    {
        if (a.size() != b.size())
            return false;
        for (std::size_t i = 0; i < a.size(); ++i)
        {
            const BodyState& p = a[i];
            const BodyState& q = b[i];
            if (p.E != q.E || !Same(p.X, q.X) || !Same(p.Y, q.Y) || !Same(p.Angle, q.Angle) || !Same(p.VX, q.VX) ||
                !Same(p.VY, q.VY) || !Same(p.Spin, q.Spin))
                return false;
            if (transforms &&
                (!Same(p.World.X, q.World.X) || !Same(p.World.Z, q.World.Z) || !Same(p.Rotation.X, q.Rotation.X) ||
                 !Same(p.Rotation.Y, q.Rotation.Y) || !Same(p.Rotation.Z, q.Rotation.Z) ||
                 !Same(p.Rotation.W, q.Rotation.W)))
                return false;
        }
        return true;
    }
} // namespace

TEST_CASE("Physics: every broad phase gives exactly the same simulation")
{
    using Mode = PhysicsSystem::BroadPhase;
    PhysicsSystem::Stats all, sweep, grid;
    std::vector<BodyState> reference = Simulate(Mode::AllPairs, 300, 20, false, &all);
    REQUIRE(reference.size() > 300u);
    // Something happened: bodies moved and touched
    CHECK(all.Contacts > 1000);
    CHECK(Identical(reference, Simulate(Mode::Sweep, 300, 20, false, &sweep), true));
    CHECK(Identical(reference, Simulate(Mode::Grid, 300, 20, false, &grid), true));
    CHECK(Identical(reference, Simulate(Mode::Automatic, 300, 20, false), true));
    // ... finding the same contacts with far fewer tests
    CHECK_EQ(sweep.Contacts, all.Contacts);
    CHECK_EQ(grid.Contacts, all.Contacts);
    CHECK(sweep.PairsTested * 3 < all.PairsTested);
    CHECK(grid.PairsTested * 3 < all.PairsTested);
    // Small scenes (sweep) too
    CHECK(Identical(Simulate(Mode::AllPairs, 40, 20, false), Simulate(Mode::Automatic, 40, 20, false), true));
}

TEST_CASE("Physics: transforms written once per step end where per sub step writes end")
{
    // A category callback makes the system write the transforms after every
    // sub step (callbacks may read them); without one they are written once
    // at the end of the step. Bodies and transforms must match bit for bit
    std::vector<BodyState> once = Simulate(PhysicsSystem::BroadPhase::Automatic, 200, 15, false);
    std::vector<BodyState> everySubStep = Simulate(PhysicsSystem::BroadPhase::Automatic, 200, 15, true);
    CHECK(Identical(once, everySubStep, true));
}

TEST_CASE("Physics: transforms follow the bodies; static and resting bodies are left alone")
{
    Fixture::FreshWorld();
    Entity wall = Body(Shape2DType::Rectangle, {0, 0, 0}, 4, 1, BodyType::Static);
    Entity rest = Body(Shape2DType::Circle, {6, 0, 6}, 1, 1, BodyType::Dynamic);
    Entity ball = Body(Shape2DType::Circle, {0, 0, 5}, 1, 1, BodyType::Dynamic);
    ECS.GetComponent<RigidBody>(ball).Velocity = Vec2(0.0f, -20.0f);
    ECS.GetComponent<RigidBody>(ball).AngularVelocity = 1.0f;
    ECS.FlushECS();
    PhysicsSystem physics;
    physics.Update(16.5f);
    for (Entity e : {wall, rest, ball})
        ECS.GetComponent<Transform>(e).IsDirty = false;
    for (int i = 0; i < 30; ++i)
        physics.Update(16.5f);

    const RigidBody& body = ECS.GetComponent<RigidBody>(ball);
    Transform& t = ECS.GetComponent<Transform>(ball);
    CHECK(t.IsDirty);
    CHECK_EQ(t.GetWorldPosition().X, body.Position.X);
    CHECK_EQ(t.GetWorldPosition().Z, body.Position.Y);
    // The ball hit the wall and bounced back
    CHECK(body.Position.Y > 1.0f);
    CHECK(body.Velocity.Y > 0.0f);
    // It turned, and the transform with it
    CHECK(std::fabs(-t.GetWorldRotation().GetPitch2D() - body.Angular) < 1e-3f);
    // The static wall and the resting body were not rewritten
    CHECK(!ECS.GetComponent<Transform>(wall).IsDirty);
    CHECK(!ECS.GetComponent<Transform>(rest).IsDirty);
}
