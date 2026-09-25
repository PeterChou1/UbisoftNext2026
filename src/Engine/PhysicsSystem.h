//---------------------------------------------------------------------------------
// PhysicsSystem.h
//---------------------------------------------------------------------------------
//
// Implements the Physics Pipeline and main class for simulating RigidBody
// Physics
//
#pragma once

#include "ColliderCallbackSystem.h"
#include "Manifolds.h"
#include "RigidBody.h"

#include <cstdint>
#include <optional>
#include <vector>

class PhysicsSystem
{
  public:
    PhysicsSystem();

    void SyncData();

    /**
     * \brief Write every body's position and rotation to its transform
     *        (after each sub step when category callbacks are registered)
     */
    void ForwardTransform();

    void Update(float deltaTime);

    void Step();

    /**
     * \brief Work done since the last ResetStats (benchmarks, tests)
     */
    struct Stats
    {
        // Body pairs whose bounding boxes were compared, and pairs touching
        long long PairsTested = 0;
        long long Contacts = 0;
        long long Steps = 0;
        // Time spent in the phases of the sub steps
        double BroadMs = 0.0;
        double NarrowMs = 0.0;
        double SolveMs = 0.0;
        double OtherMs = 0.0;
    };
    const Stats& GetStats() const { return m_Stats; }

    /**
     * \brief How candidate pairs are found. All of them give the same pairs
     *        (so the same simulation); Automatic picks the fastest
     */
    enum class BroadPhase
    {
        Automatic, // Sweep below GRID_BODIES bodies, Grid above
        Sweep,     // sort and sweep along X
        Grid,      // uniform grid
        AllPairs   // every body against every body (reference)
    };
    void SetBroadPhase(BroadPhase mode) { m_BroadPhase = mode; }
    static constexpr std::size_t GRID_BODIES = 256;
    void ResetStats() { m_Stats = Stats{}; }

  private:
    Stats m_Stats;
    void AccumulateRotation();
    void FinishTransforms();
    // Per body (same order as m_RigidBodies): its transform, where it was at
    // the start of the step, and whether it turned
    std::vector<Transform*> m_Transforms;
    std::vector<Vec2> m_StartPositions;
    std::vector<std::uint8_t> m_Rotated;
    float m_Accumulate{};
    std::shared_ptr<ColliderCallbackSystem> m_CallbackSystem;
    std::vector<std::pair<Entity, RigidBody&>> m_RigidBodies;
    std::vector<Manifold> m_Collisions;
    // Broad phase scratch (kept between steps to avoid allocations)
    std::vector<std::uint32_t> m_SweepOrder;
    std::vector<float> m_SweepKeys;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> m_Pairs;
    struct SweepBox
    {
        float Key;
        float MinX, MaxX, MinY, MaxY;
        std::uint32_t Index;
        bool Static;
    };
    std::vector<SweepBox> m_SweepBoxes;
    BroadPhase m_BroadPhase = BroadPhase::Automatic;
    void GridPairs();
    void AllPairs();
    // Grid: first entry of every cell in m_CellBodies, bodies per cell, and
    // the bodies too big for it (indices into m_SweepBoxes, sorted)
    std::vector<std::size_t> m_CellStart;
    std::vector<std::size_t> m_CellFill;
    std::vector<std::uint32_t> m_CellBodies;
    std::vector<std::uint32_t> m_BigBodies;
    static constexpr int BIG_BODY_CELLS = 16;
    std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> m_ChunkPairs;
    std::vector<std::optional<Manifold>> m_Narrow;
    std::vector<std::size_t> m_ChunkIds;
    // Split the work in CHUNKS pieces for the worker threads once there are
    // this many bodies (sweep) / candidate pairs (narrow phase)
    static constexpr std::size_t PARALLEL_BODIES = 4096;
    static constexpr std::size_t PARALLEL_PAIRS = 2048;
    static constexpr std::size_t CHUNKS = 16;
    template <typename Fn>
    void RunChunks(std::size_t chunks, Fn&& work);
    // How much times the physics iteration is run per time step
    // increase for more accuracy at the cost performance
    constexpr static float STEP_ITERATION = 10;
};
