//---------------------------------------------------------------------------------
// PhysicsBenchmark.cpp
//---------------------------------------------------------------------------------
//
// Times the physics system (PhysicsSystem::Update, one 16 ms game frame =
// 10 sub steps) with more and more bodies, in three situations:
//
//   scatter  bodies spread over a large walled arena, moving in every
//            direction (few contacts: measures the broad phase)
//   pile     bodies packed tightly in a small arena (many contacts: measures
//            the narrow phase and the solver)
//   walls    many static walls (a maze) and moving bodies between them
//
// For each run it prints the average and worst frame time, the collision
// pairs tested and found per frame, and a checksum of the final positions
// (it must not change when the physics code is only made faster).
//
//     cmake --build build/tests --target physics_benchmark        (all sizes)
//     PhysicsBenchmark [max bodies] [frames] [auto|sweep|grid|all]
//
// Build it with optimisations for meaningful numbers
// (-DCMAKE_BUILD_TYPE=Release).
//
#include "ECSManager.h"
#include "PhysicsSystem.h"
#include "TestEnvironment.h"
#include "World/SceneObjects.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

extern ECSManager ECS;

namespace
{
    using SceneObjects::BodyType;

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

    void Arena(float half)
    {
        Body(Shape2DType::Rectangle, {0, 0, half}, half * 2.0f + 2.0f, 1.0f, BodyType::Static);
        Body(Shape2DType::Rectangle, {0, 0, -half}, half * 2.0f + 2.0f, 1.0f, BodyType::Static);
        Body(Shape2DType::Rectangle, {half, 0, 0}, 1.0f, half * 2.0f + 2.0f, BodyType::Static);
        Body(Shape2DType::Rectangle, {-half, 0, 0}, 1.0f, half * 2.0f + 2.0f, BodyType::Static);
    }

    // Every third body a box, every third a hexagon, the rest circles
    void MovingBody(int i, const Vec3& at, std::mt19937& random)
    {
        const Shape2DType types[] = {
                Shape2DType::Circle, Shape2DType::Rectangle, Shape2DType::Polygon};
        std::uniform_real_distribution<float> angle(0.0f, 360.0f);
        Entity e = Body(types[i % 3], at, 0.8f, 0.8f, BodyType::Dynamic, angle(random));
        std::uniform_real_distribution<float> speed(-6.0f, 6.0f);
        ECS.GetComponent<RigidBody>(e).Velocity = Vec2(speed(random), speed(random));
    }

    void Scatter(int count, std::mt19937& random)
    {
        // About 6 square units per body
        float half = std::sqrt(static_cast<float>(count) * 6.0f) * 0.5f + 2.0f;
        Arena(half);
        std::uniform_real_distribution<float> spot(-half + 1.5f, half - 1.5f);
        for (int i = 0; i < count; ++i)
            MovingBody(i, {spot(random), 0, spot(random)}, random);
    }

    void Pile(int count, std::mt19937& random)
    {
        // A grid of bodies almost touching, in an arena just big enough
        int side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
        float spacing = 0.85f;
        float half = side * spacing * 0.5f + 1.0f;
        Arena(half);
        for (int i = 0; i < count; ++i)
        {
            float x = (static_cast<float>(i % side) - side * 0.5f + 0.5f) * spacing;
            float z = (static_cast<float>(i / side) - side * 0.5f + 0.5f) * spacing;
            MovingBody(i, {x, 0, z}, random);
        }
    }

    void Walls(int count, std::mt19937& random)
    {
        float half = std::sqrt(static_cast<float>(count) * 8.0f) * 0.5f + 2.0f;
        Arena(half);
        // A static wall every 4 units on a grid, a moving body in between
        std::uniform_real_distribution<float> turn(0.0f, 180.0f);
        int placed = 0;
        for (float x = -half + 3.0f; x < half - 2.0f && placed < count; x += 4.0f)
        {
            for (float z = -half + 3.0f; z < half - 2.0f && placed < count; z += 4.0f)
            {
                Body(Shape2DType::Rectangle, {x, 0, z}, 2.0f, 0.4f, BodyType::Static, turn(random));
                MovingBody(placed++, {x + 2.0f, 0, z + 2.0f}, random);
            }
        }
    }

    struct Result
    {
        double AverageMs = 0.0;
        double WorstMs = 0.0;
        double PairsTested = 0.0;
        double Contacts = 0.0;
        double Checksum = 0.0;
        int Bodies = 0;
        double BroadMs = 0.0, NarrowMs = 0.0, SolveMs = 0.0, OtherMs = 0.0;
    };

    PhysicsSystem::BroadPhase g_Mode = PhysicsSystem::BroadPhase::Automatic;

    Result Run(void (*build)(int, std::mt19937&), int count, int frames)
    {
        ECS.ClearWorld();
        ECS.FlushECS();
        std::mt19937 random(1234);
        build(count, random);
        ECS.FlushECS();
        PhysicsSystem physics;
        physics.SetBroadPhase(g_Mode);
        Result result;
        result.Bodies = static_cast<int>(ECS.Visit<RigidBody>().size());
        // One frame to settle the first contacts, not timed
        physics.Update(16.0f);
        PhysicsSystem::Stats total;
        for (int frame = 0; frame < frames; ++frame)
        {
            physics.ResetStats();
            auto start = std::chrono::steady_clock::now();
            physics.Update(16.0f);
            auto end = std::chrono::steady_clock::now();
            double ms = std::chrono::duration<double, std::milli>(end - start).count();
            result.AverageMs += ms;
            result.WorstMs = std::max(result.WorstMs, ms);
            total.PairsTested += physics.GetStats().PairsTested;
            total.Contacts += physics.GetStats().Contacts;
            result.BroadMs += physics.GetStats().BroadMs / frames;
            result.NarrowMs += physics.GetStats().NarrowMs / frames;
            result.SolveMs += physics.GetStats().SolveMs / frames;
            result.OtherMs += physics.GetStats().OtherMs / frames;
        }
        result.AverageMs /= frames;
        result.PairsTested = static_cast<double>(total.PairsTested) / frames;
        result.Contacts = static_cast<double>(total.Contacts) / frames;
        for (Entity e : ECS.Visit<RigidBody>())
        {
            const RigidBody& body = ECS.GetComponent<RigidBody>(e);
            result.Checksum += body.Position.X * 0.7 + body.Position.Y * 1.3 + body.Angular * 0.1;
        }
        return result;
    }
} // namespace

int main(int argc, char** argv)
{
    int maxBodies = argc > 1 ? std::atoi(argv[1]) : 4000;
    int frames = argc > 2 ? std::atoi(argv[2]) : 30;
    std::string mode = argc > 3 ? argv[3] : "auto";
    if (mode == "sweep")
        g_Mode = PhysicsSystem::BroadPhase::Sweep;
    else if (mode == "grid")
        g_Mode = PhysicsSystem::BroadPhase::Grid;
    else if (mode == "all")
        g_Mode = PhysicsSystem::BroadPhase::AllPairs;
    std::printf("broad phase: %s\n", mode.c_str());
    TestEnvironment::Init();

    struct Scenario
    {
        const char* Name;
        void (*Build)(int, std::mt19937&);
    };
    const Scenario scenarios[] = {{"scatter", Scatter}, {"pile", Pile}, {"walls", Walls}};
    std::printf("%-8s %7s %9s %9s %12s %9s %16s   %s\n",
                "scenario",
                "bodies",
                "avg ms",
                "worst ms",
                "pairs tested",
                "contacts",
                "checksum",
                "ms in: broad / narrow / contacts / solve+integrate");
    for (const Scenario& scenario : scenarios)
    {
        for (int count : {100, 250, 500, 1000, 2000, 4000, 8000})
        {
            if (count > maxBodies)
                break;
            Result r = Run(scenario.Build, count, frames);
            std::printf("%-8s %7d %9.3f %9.3f %12.0f %9.1f %16.6f   %.2f / %.2f / %.2f / %.2f\n",
                        scenario.Name,
                        r.Bodies,
                        r.AverageMs,
                        r.WorstMs,
                        r.PairsTested,
                        r.Contacts,
                        r.Checksum,
                        r.BroadMs,
                        r.NarrowMs,
                        r.OtherMs,
                        r.SolveMs);
            std::fflush(stdout);
        }
    }
    return 0;
}
