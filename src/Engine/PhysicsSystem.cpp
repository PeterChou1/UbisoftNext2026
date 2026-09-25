#include "PhysicsSystem.h"

#include "Concurrent.h"
#include "ECSManager.h"
#include "Manifolds.h"
#include "RigidBody.h"
#include "Transform.h"
#include "stdafx.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>

extern ECSManager ECS;

// Step time of each physics simulation decrease for more accuracy
// at the cost of performance
constexpr float StepTime = 16.0f;
constexpr float dt = StepTime / 1000.0f;
// Maximum time a physics step will process this is to prevent
// a massive lag spike from overwhelming the physics logic
constexpr float MaxTime = 50.0f;
// How strong you want the gravity to be
constexpr float GravityScale = 5.0f;
Vec2 Gravity = Vec2(0.0f, -10.0f * GravityScale);

PhysicsSystem::PhysicsSystem()
{
    m_CallbackSystem = ECS.GetResource<ColliderCallbackSystem>();
}

void PhysicsSystem::SyncData()
{
    // Sync Transform -> RigidBody
    // Sync ECS -> m_RigidBodies
    m_RigidBodies.clear();
    m_Transforms.clear();
    m_StartPositions.clear();
    m_Rotated.clear();
    for (auto& e : ECS.Visit<RigidBody, Transform>())
    {
        RigidBody& rigidbody = ECS.GetComponent<RigidBody>(e);
        rigidbody.Color = Vec3(1.0f, 0.0f, 0.0f);
        m_RigidBodies.emplace_back(e, rigidbody);
        Transform& transform = ECS.GetComponent<Transform>(e);
        // (component storage is a fixed array: the pointer stays valid)
        m_Transforms.push_back(&transform);
        rigidbody.SyncTransform(transform);
        rigidbody.RecomputeGeometry();
        m_StartPositions.push_back(rigidbody.Position);
        m_Rotated.push_back(0);
        if (!rigidbody.Initialized)
            rigidbody.Initialized = true;
    }
}

void PhysicsSystem::ForwardTransform()
{
    // Category callbacks run between sub steps and may read the transforms:
    // keep them up to date after every sub step
    for (std::size_t i = 0; i < m_RigidBodies.size(); ++i)
    {
        // A static body without a parent never moves (one with a parent is
        // written back: it stays where physics has it, like a moving body)
        if (m_RigidBodies[i].second.InvMass() == 0.0f && m_Transforms[i]->Parent == NULL_ENTITY)
            continue;
        m_RigidBodies[i].second.ForwardTransform(*m_Transforms[i]);
    }
}

void PhysicsSystem::AccumulateRotation()
{
    // Without callbacks nothing reads the transforms until the step ends:
    // only the rotation is accumulated per sub step (the same quaternion
    // products, in the same order, as ForwardTransform), the matrices are
    // rebuilt once in FinishTransforms
    for (std::size_t i = 0; i < m_RigidBodies.size(); ++i)
    {
        const RigidBody& body = m_RigidBodies[i].second;
        if (!body.Collidable || body.InvMass() == 0.0f || body.AngularDelta == 0.0f)
            continue;
        Transform& transform = *m_Transforms[i];
        transform.LocalRotation *=
                Quat(RigidBody::RotationAxis(transform.Plane), RigidBody::TransformTurn(transform.Plane, body.AngularDelta));
        m_Rotated[i] = 1;
    }
}

void PhysicsSystem::FinishTransforms()
{
    for (std::size_t i = 0; i < m_RigidBodies.size(); ++i)
    {
        const RigidBody& body = m_RigidBodies[i].second;
        if (!body.Collidable)
            continue;
        Transform& transform = *m_Transforms[i];
        // Bodies with a parent are always written back: they stay where
        // physics has them even when their parent moved. A root body that is
        // static, or did not move or turn, is left alone
        if (transform.Parent == NULL_ENTITY)
        {
            const Vec2& start = m_StartPositions[i];
            if (body.InvMass() == 0.0f ||
                (!m_Rotated[i] && body.Position.X == start.X && body.Position.Y == start.Y))
                continue;
        }
        body.ForwardPosition(transform);
        transform.RebuildAffine();
    }
}

void PhysicsSystem::Update(float deltaTime)
{
    m_Accumulate += deltaTime;

    if (m_Accumulate > MaxTime)
        m_Accumulate = MaxTime;

    // Physics is updated once every 16ms
    while (m_Accumulate > StepTime)
    {
        SyncData();
        const bool callbacks = m_CallbackSystem->HasCallbacks();
        for (int i = 0; i < STEP_ITERATION; i++)
        {
            Step();
            if (callbacks)
                ForwardTransform();
            else
                AccumulateRotation();
            m_CallbackSystem->Update();
        }
        if (!callbacks)
            FinishTransforms();
        // One fixed step consumed (was MaxTime, which ran the simulation at
        // roughly a third of real time)
        m_Accumulate -= StepTime;
    }
}

void PhysicsSystem::AllPairs()
{
    // The original all pairs test (reference for the tests)
    const std::size_t count = m_RigidBodies.size();
    for (std::uint32_t a = 0; a < count; ++a)
    {
        RigidBody& ra = m_RigidBodies[a].second;
        for (std::uint32_t b = a + 1; b < count; ++b)
        {
            RigidBody& rb = m_RigidBodies[b].second;
            if (ra.InvMass() == 0.0f && rb.InvMass() == 0.0f)
                continue;
            ++m_Stats.PairsTested;
            if (AABBTest(ra.RigidBodyAABB, rb.RigidBodyAABB))
                m_Pairs.emplace_back(a, b);
        }
    }
}

void PhysicsSystem::GridPairs()
{
    // A uniform grid over the boxes (m_SweepBoxes: every body's box). Each
    // body is listed in the cells its box covers; bodies are only compared
    // with the others in the same cells. A pair is kept in one cell only:
    // the one holding the corner (largest min x, largest min y) of the two
    // boxes, which lies in both boxes when they overlap. Bodies covering
    // many cells (long walls) are compared with every body instead
    const std::size_t count = m_SweepBoxes.size();
    if (count == 0)
        return;
    float minX = std::numeric_limits<float>::infinity(), minY = minX;
    float maxX = -minX, maxY = -minX;
    double extent = 0.0;
    std::size_t finite = 0;
    for (const SweepBox& box : m_SweepBoxes)
    {
        if (!std::isfinite(box.MinX) || !std::isfinite(box.MaxX) || !std::isfinite(box.MinY) ||
            !std::isfinite(box.MaxY))
            continue;
        minX = std::min(minX, box.MinX);
        minY = std::min(minY, box.MinY);
        maxX = std::max(maxX, box.MaxX);
        maxY = std::max(maxY, box.MaxY);
        if (!box.Static)
        {
            extent += std::max(box.MaxX - box.MinX, box.MaxY - box.MinY);
            ++finite;
        }
    }
    if (!(minX <= maxX))
        return;
    // Cells about twice the size of a typical moving body, at most a few
    // cells per body
    float cell = finite > 0 ? static_cast<float>(2.0 * extent / static_cast<double>(finite)) : 1.0f;
    cell = std::max(cell, 1e-3f);
    const float width = std::max(maxX - minX, cell);
    const float height = std::max(maxY - minY, cell);
    const double maxCells = 4.0 * static_cast<double>(count) + 64.0;
    while (static_cast<double>(std::ceil(width / cell)) * static_cast<double>(std::ceil(height / cell)) > maxCells)
        cell *= 1.5f;
    const int columns = static_cast<int>(std::ceil(width / cell)) + 1;
    const int rows = static_cast<int>(std::ceil(height / cell)) + 1;
    auto column = [&](float x) { return std::clamp(static_cast<int>(std::floor((x - minX) / cell)), 0, columns - 1); };
    auto row = [&](float y) { return std::clamp(static_cast<int>(std::floor((y - minY) / cell)), 0, rows - 1); };

    // Count, then fill (one flat array, no per cell vectors)
    const std::size_t cells = static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    m_CellStart.assign(cells + 1, 0);
    m_BigBodies.clear();
    auto covers = [&](const SweepBox& box, int& c0, int& c1, int& r0, int& r1) {
        c0 = column(box.MinX);
        c1 = column(box.MaxX);
        r0 = row(box.MinY);
        r1 = row(box.MaxY);
    };
    for (std::size_t i = 0; i < count; ++i)
    {
        const SweepBox& box = m_SweepBoxes[i];
        if (!std::isfinite(box.MinX) || !std::isfinite(box.MaxX) || !std::isfinite(box.MinY) ||
            !std::isfinite(box.MaxY))
            continue;
        int c0, c1, r0, r1;
        covers(box, c0, c1, r0, r1);
        if ((c1 - c0 + 1) * (r1 - r0 + 1) > BIG_BODY_CELLS)
        {
            m_BigBodies.push_back(static_cast<std::uint32_t>(i));
            continue;
        }
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                ++m_CellStart[static_cast<std::size_t>(r) * columns + c + 1];
    }
    for (std::size_t c = 0; c < cells; ++c)
        m_CellStart[c + 1] += m_CellStart[c];
    m_CellBodies.resize(m_CellStart[cells]);
    m_CellFill.assign(m_CellStart.begin(), m_CellStart.end() - 1);
    std::size_t big = 0;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (big < m_BigBodies.size() && m_BigBodies[big] == i)
        {
            ++big;
            continue;
        }
        const SweepBox& box = m_SweepBoxes[i];
        if (!std::isfinite(box.MinX) || !std::isfinite(box.MaxX) || !std::isfinite(box.MinY) ||
            !std::isfinite(box.MaxY))
            continue;
        int c0, c1, r0, r1;
        covers(box, c0, c1, r0, r1);
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                m_CellBodies[m_CellFill[static_cast<std::size_t>(r) * columns + c]++] = static_cast<std::uint32_t>(i);
    }

    auto overlap = [](const SweepBox& a, const SweepBox& b) {
        // AABBTest
        return !(a.MaxX < b.MinX || a.MinX > b.MaxX || a.MaxY < b.MinY || a.MinY > b.MaxY);
    };
    auto keep = [&](const SweepBox& a, const SweepBox& b) {
        m_Pairs.emplace_back(std::min(a.Index, b.Index), std::max(a.Index, b.Index));
    };
    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            const std::size_t cellIndex = static_cast<std::size_t>(r) * columns + c;
            const std::size_t first = m_CellStart[cellIndex];
            const std::size_t last = m_CellStart[cellIndex + 1];
            for (std::size_t i = first; i < last; ++i)
            {
                const SweepBox& a = m_SweepBoxes[m_CellBodies[i]];
                for (std::size_t j = i + 1; j < last; ++j)
                {
                    const SweepBox& b = m_SweepBoxes[m_CellBodies[j]];
                    // don't bother resolving collision between two infinite mass
                    if (a.Static && b.Static)
                        continue;
                    ++m_Stats.PairsTested;
                    if (!overlap(a, b))
                        continue;
                    // Only in the cell of the overlap's corner
                    if (column(std::max(a.MinX, b.MinX)) != c || row(std::max(a.MinY, b.MinY)) != r)
                        continue;
                    keep(a, b);
                }
            }
        }
    }
    // Big bodies against every body (once per pair of big bodies)
    for (std::size_t g = 0; g < m_BigBodies.size(); ++g)
    {
        const SweepBox& a = m_SweepBoxes[m_BigBodies[g]];
        for (std::size_t i = 0; i < count; ++i)
        {
            if (i == m_BigBodies[g])
                continue;
            if (std::binary_search(m_BigBodies.begin(), m_BigBodies.end(), static_cast<std::uint32_t>(i)) &&
                i < m_BigBodies[g])
                continue;
            const SweepBox& b = m_SweepBoxes[i];
            if (a.Static && b.Static)
                continue;
            ++m_Stats.PairsTested;
            if (overlap(a, b))
                keep(a, b);
        }
    }
}

template <typename Fn>
void PhysicsSystem::RunChunks(std::size_t chunks, Fn&& work)
{
    if (chunks <= 1)
    {
        work(0);
        return;
    }
    m_ChunkIds.resize(chunks);
    for (std::size_t i = 0; i < chunks; ++i)
        m_ChunkIds[i] = i;
    Concurrent::ForEach(m_ChunkIds.begin(), m_ChunkIds.end(), [&](std::size_t chunk) { work(chunk); });
}

void PhysicsSystem::Step()
{
    m_Collisions.clear();
    ++m_Stats.Steps;
    float deltaTime = dt / STEP_ITERATION;

    // Reset is intersecting
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rb = collisionPair.second;
        rb.IsIntersecting = false;
    }

    auto broadStart = std::chrono::steady_clock::now();
    // Broad phase: sort and sweep along X. Bodies are sorted by the left
    // edge of their bounding box; each one is only compared with the bodies
    // that start before its right edge (instead of with every other body).
    // The candidate pairs are then put back in the order the all-pairs loop
    // visited them (by body index), so contacts are solved in the same order
    // and the simulation gives exactly the same results
    const std::size_t count = m_RigidBodies.size();
    BroadPhase mode = m_BroadPhase;
    if (mode == BroadPhase::Automatic)
        mode = count >= GRID_BODIES ? BroadPhase::Grid : BroadPhase::Sweep;
    const bool sweepMode = mode == BroadPhase::Sweep;
    m_SweepKeys.resize(count);
    for (std::size_t i = 0; sweepMode && i < count; ++i)
    {
        float key = m_RigidBodies[i].second.RigidBodyAABB.Min.X;
        // A broken (NaN) box sorts last instead of breaking the sort
        m_SweepKeys[i] = std::isnan(key) ? std::numeric_limits<float>::infinity() : key;
    }
    auto before = [&](std::uint32_t a, std::uint32_t b) {
        return m_SweepKeys[a] < m_SweepKeys[b] || (m_SweepKeys[a] == m_SweepKeys[b] && a < b);
    };
    if (!sweepMode)
    {
        // (the grid does not need the order)
    }
    else if (m_SweepOrder.size() == count)
    {
        // Bodies move a little per sub step: last order is almost sorted,
        // insertion sort finishes it in about linear time (the order is a
        // strict total order, so any sort gives the same result)
        for (std::size_t i = 1; i < count; ++i)
        {
            std::uint32_t value = m_SweepOrder[i];
            std::size_t j = i;
            while (j > 0 && before(value, m_SweepOrder[j - 1]))
            {
                m_SweepOrder[j] = m_SweepOrder[j - 1];
                --j;
            }
            m_SweepOrder[j] = value;
        }
    }
    else
    {
        m_SweepOrder.resize(count);
        for (std::size_t i = 0; i < count; ++i)
            m_SweepOrder[i] = static_cast<std::uint32_t>(i);
        std::sort(m_SweepOrder.begin(), m_SweepOrder.end(), before);
    }
    // The sweep, and the narrow phase below, run on the engine's worker
    // threads when there is enough work: each chunk writes only its own
    // slots, and the results are used in body order, so the simulation is
    // the same with any number of threads
    const bool parallel = count >= PARALLEL_BODIES;
    const std::size_t chunks = parallel ? CHUNKS : 1;
    m_ChunkPairs.resize(chunks);
    std::vector<long long> tested(chunks, 0);
    // The boxes copied in sweep order into one compact array: the inner loop
    // below then reads memory in order instead of jumping between bodies
    m_SweepBoxes.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint32_t index = sweepMode ? m_SweepOrder[i] : static_cast<std::uint32_t>(i);
        const RigidBody& body = m_RigidBodies[index].second;
        SweepBox& box = m_SweepBoxes[i];
        box.Key = sweepMode ? m_SweepKeys[index] : 0.0f;
        box.MinX = body.RigidBodyAABB.Min.X;
        box.MaxX = body.RigidBodyAABB.Max.X;
        box.MinY = body.RigidBodyAABB.Min.Y;
        box.MaxY = body.RigidBodyAABB.Max.Y;
        box.Index = index;
        box.Static = body.InvMass() == 0.0f;
    }
    auto sweep = [&](std::size_t chunk) {
        auto& pairs = m_ChunkPairs[chunk];
        pairs.clear();
        const std::size_t first = count * chunk / chunks;
        const std::size_t last = count * (chunk + 1) / chunks;
        const SweepBox* boxes = m_SweepBoxes.data();
        long long tests = 0;
        for (std::size_t s = first; s < last; ++s)
        {
            const SweepBox& a = boxes[s];
            for (std::size_t t = s + 1; t < count; ++t)
            {
                const SweepBox& b = boxes[t];
                // Every later body starts to the right of this one's box
                if (b.Key > a.MaxX)
                    break;
                // don't bother resolving collision between two infinite mass
                if (a.Static && b.Static)
                    continue;
                ++tests;
                // AABBTest
                if (a.MaxX < b.MinX || a.MinX > b.MaxX || a.MaxY < b.MinY || a.MinY > b.MaxY)
                    continue;
                pairs.emplace_back(std::min(a.Index, b.Index), std::max(a.Index, b.Index));
            }
        }
        tested[chunk] = tests;
    };
    m_Pairs.clear();
    if (mode == BroadPhase::Grid)
        GridPairs();
    else if (mode == BroadPhase::AllPairs)
        AllPairs();
    else
    {
        RunChunks(chunks, sweep);
        for (std::size_t chunk = 0; chunk < chunks; ++chunk)
        {
            m_Pairs.insert(m_Pairs.end(), m_ChunkPairs[chunk].begin(), m_ChunkPairs[chunk].end());
            m_Stats.PairsTested += tested[chunk];
        }
    }
    std::sort(m_Pairs.begin(), m_Pairs.end());

    auto narrowStart = std::chrono::steady_clock::now();
    // Narrow phase: the exact test of every candidate pair (reads the bodies
    // only), each into its own slot
    const std::size_t pairCount = m_Pairs.size();
    m_Narrow.clear();
    m_Narrow.resize(pairCount);
    auto narrow = [&](std::size_t chunk, std::size_t chunkCount) {
        const std::size_t first = pairCount * chunk / chunkCount;
        const std::size_t last = pairCount * (chunk + 1) / chunkCount;
        for (std::size_t k = first; k < last; ++k)
        {
            const auto& pair = m_Pairs[k];
            m_Narrow[k].emplace(m_RigidBodies[pair.first].first,
                                m_RigidBodies[pair.second].first,
                                m_RigidBodies[pair.first].second,
                                m_RigidBodies[pair.second].second);
        }
    };
    const std::size_t narrowChunks = pairCount >= PARALLEL_PAIRS ? CHUNKS : 1;
    RunChunks(narrowChunks, [&](std::size_t chunk) { narrow(chunk, narrowChunks); });

    auto contactsStart = std::chrono::steady_clock::now();
    // Contacts, in body order
    for (std::size_t k = 0; k < pairCount; ++k)
    {
        Manifold& m = *m_Narrow[k];
        if (!m.Collided)
            continue;
        Entity e1 = m.idA;
        Entity e2 = m.idB;
        RigidBody& r1 = m.A;
        RigidBody& r2 = m.B;
        r1.IsIntersecting = true;
        r2.IsIntersecting = true;
        ++m_Stats.Contacts;
        m_CallbackSystem->SubmitContact(e1, e2);
        if (m_CallbackSystem->HasRegisterCallback({r1.Category, r2.Category}))
        {
            m_CallbackSystem->SubmitForCallback(e1, e2);
        }
        m_Collisions.push_back(std::move(m));
    }

    auto solveStart = std::chrono::steady_clock::now();
    // Integrate Forces
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rigidbody = collisionPair.second;
        if (rigidbody.InvMass() == 0.0 || !rigidbody.Collidable)
            continue;

        // Apply Gravity
        // rigidbody.Velocity += Gravity * deltaTime;
        // Calculate normal force (assuming flat surface and gravity only)

        const Vec2 normalForce(0.0f, Gravity.Y);
        // Calculate friction force
        Vec2 frictionForce;
        if (rigidbody.Velocity.GetMagnitudeSquared() > 0)
        {
            // Kinetic friction
            Vec2 rbVelocity = rigidbody.Velocity * -1.0f;
            rbVelocity.Normalize();
            frictionForce = rbVelocity * normalForce.GetMagnitude() * rigidbody.DynamicFriction;
        }
        else
        {
            frictionForce = Vec2(0.0f, 0.0f);
        }
        rigidbody.Velocity += frictionForce * deltaTime;
    }

    // Resolve Collision
    for (auto& manifold : m_Collisions)
    {
        manifold.ResolveCollisionAngular();
    }

    // Integrate Velocity
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rigidbody = collisionPair.second;
        rigidbody.IntegrateVelocityAngular(deltaTime);
    }

    // Correct Position
    for (auto& manifold : m_Collisions)
    {
        manifold.PositionCorrection();
    }

    // Clear All Forces
    // Sync Rigid Body -> Transform
    for (auto& collisionPair : m_RigidBodies)
    {
        RigidBody& rigidbody = collisionPair.second;
        rigidbody.Force = Vec2(0.0f, 0.0f);
        rigidbody.RecomputeGeometry();
    }
    auto end = std::chrono::steady_clock::now();
    auto ms = [](auto from, auto to) { return std::chrono::duration<double, std::milli>(to - from).count(); };
    m_Stats.BroadMs += ms(broadStart, narrowStart);
    m_Stats.NarrowMs += ms(narrowStart, contactsStart);
    m_Stats.SolveMs += ms(solveStart, end);
    m_Stats.OtherMs += ms(contactsStart, solveStart);
}
