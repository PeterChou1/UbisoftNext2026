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

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class PhysicsSystem
{
  public:
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
    static constexpr std::size_t GRID_BODIES = 256;

    PhysicsSystem();

    /**
     * \brief Run the fixed physics steps due after deltaTime (ms)
     */
    void Update(float deltaTime);

    const Stats& GetStats() const { return m_Stats; }
    void ResetStats() { m_Stats = Stats{}; }
    void SetBroadPhase(BroadPhase mode) { m_BroadPhase = mode; }

  private:
    // A body's bounding box, copied into one compact array for the broad phase
    struct SweepBox
    {
        float Key;
        float MinX, MaxX, MinY, MaxY;
        std::uint32_t Index;
        bool Static;

        bool IsFinite() const
        {
            return std::isfinite(MinX) && std::isfinite(MaxX) && std::isfinite(MinY) &&
                   std::isfinite(MaxY);
        }

        // AABBTest
        bool Overlaps(const SweepBox& other) const
        {
            return !(MaxX < other.MinX || MinX > other.MaxX || MaxY < other.MinY ||
                     MinY > other.MaxY);
        }
    };

    /**
     * \brief Read every body (and its transform) from the ECS
     */
    void SyncData();

    /**
     * \brief One physics sub step: broad phase, narrow phase, contacts, solver
     */
    void Step();

    /**
     * \brief Write every body's position and rotation to its transform
     *        (after each sub step when category callbacks are registered)
     */
    void ForwardTransform();
    void AccumulateRotation();
    void FinishTransforms();

    // Broad phase: the candidate pairs (m_Pairs, sorted body index pairs)
    void FindPairs();
    void SortSweepOrder();
    void SweepPairs();
    void GridPairs();
    void AllPairs();

    void NarrowPhase();
    void CollectContacts();
    void Solve(float deltaTime);

    template <typename Fn>
    void RunChunks(std::size_t chunks, Fn&& work);

    // Split the work in CHUNKS pieces for the worker threads once there are
    // this many bodies (sweep) / candidate pairs (narrow phase)
    static constexpr std::size_t PARALLEL_BODIES = 4096;
    static constexpr std::size_t PARALLEL_PAIRS = 2048;
    static constexpr std::size_t CHUNKS = 16;
    // Grid: bodies covering more cells are compared with every body instead
    static constexpr int BIG_BODY_CELLS = 16;

    Stats m_Stats;
    BroadPhase m_BroadPhase = BroadPhase::Automatic;
    float m_AccumulatedTime{};
    std::shared_ptr<ColliderCallbackSystem> m_CallbackSystem;
    std::vector<std::pair<Entity, RigidBody&>> m_RigidBodies;
    // Per body (same order as m_RigidBodies): its transform, where it was at
    // the start of the step, and whether it turned
    std::vector<Transform*> m_Transforms;
    std::vector<Vec2> m_StartPositions;
    std::vector<std::uint8_t> m_Rotated;
    std::vector<Manifold> m_Collisions;

    // Broad phase scratch (kept between steps to avoid allocations)
    std::vector<std::uint32_t> m_SweepOrder;
    std::vector<float> m_SweepKeys;
    std::vector<SweepBox> m_SweepBoxes;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> m_Pairs;
    std::vector<std::vector<std::pair<std::uint32_t, std::uint32_t>>> m_ChunkPairs;
    // Grid: first entry of every cell in m_CellBodies, bodies per cell, and
    // the bodies too big for it (indices into m_SweepBoxes, sorted)
    std::vector<std::size_t> m_CellStart;
    std::vector<std::size_t> m_CellFill;
    std::vector<std::uint32_t> m_CellBodies;
    std::vector<std::uint32_t> m_BigBodies;
    // Narrow phase result of every candidate pair
    std::vector<std::optional<Manifold>> m_Narrow;
    std::vector<std::size_t> m_ChunkIds;
};
