#include "PhysicsSystem.h"

#include "Concurrent.h"
#include "ECSManager.h"
#include "Transform.h"
#include "stdafx.h"

#include <algorithm>
#include <chrono>
#include <limits>

extern ECSManager ECS;

namespace
{
    // Time of one physics step (ms): decrease for more accuracy at the cost
    // of performance
    constexpr float STEP_TIME = 16.0f;
    // Maximum time a physics update will process, so that a massive lag spike
    // does not overwhelm the physics
    constexpr float MAX_ACCUMULATED_TIME = 50.0f;
    // Sub steps (solver iterations) per step: increase for more accuracy at
    // the cost of performance
    constexpr int SUB_STEPS = 10;
    constexpr float SUB_STEP_SECONDS = STEP_TIME / 1000.0f / SUB_STEPS;
    // Gravity (10 times GRAVITY_SCALE) pressing the bodies on the ground
    // plane: the normal force of their friction
    constexpr float GRAVITY_SCALE = 5.0f;
    constexpr float NORMAL_FORCE = 10.0f * GRAVITY_SCALE;

    std::pair<std::uint32_t, std::uint32_t> OrderedPair(std::uint32_t a, std::uint32_t b)
    {
        return {std::min(a, b), std::max(a, b)};
    }
} // namespace

PhysicsSystem::PhysicsSystem()
{
    m_CallbackSystem = ECS.GetResource<ColliderCallbackSystem>();
}

void PhysicsSystem::Update(float deltaTime)
{
    m_AccumulatedTime = std::min(m_AccumulatedTime + deltaTime, MAX_ACCUMULATED_TIME);
    while (m_AccumulatedTime > STEP_TIME)
    {
        SyncData();
        const bool callbacks = m_CallbackSystem->HasCallbacks();
        for (int i = 0; i < SUB_STEPS; i++)
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
        m_AccumulatedTime -= STEP_TIME;
    }
}

void PhysicsSystem::SyncData()
{
    m_RigidBodies.clear();
    m_Transforms.clear();
    m_StartPositions.clear();
    m_Rotated.clear();
    for (Entity e : ECS.Visit<RigidBody, Transform>())
    {
        RigidBody& body = ECS.GetComponent<RigidBody>(e);
        Transform& transform = ECS.GetComponent<Transform>(e);
        body.Color = Vec3(1.0f, 0.0f, 0.0f);
        m_RigidBodies.emplace_back(e, body);
        // (component storage is a fixed array: the pointer stays valid)
        m_Transforms.push_back(&transform);
        body.SyncTransform(transform);
        body.RecomputeGeometry();
        m_StartPositions.push_back(body.Position);
        m_Rotated.push_back(0);
        body.Initialized = true;
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
        if (m_RigidBodies[i].second.IsStatic() && m_Transforms[i]->Parent == NULL_ENTITY)
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
        if (!body.Collidable || body.IsStatic() || body.AngularDelta == 0.0f)
            continue;
        Transform& transform = *m_Transforms[i];
        transform.LocalRotation *=
                Quat(RigidBody::RotationAxis(transform.Plane),
                     RigidBody::TransformTurn(transform.Plane, body.AngularDelta));
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
            if (body.IsStatic() ||
                (!m_Rotated[i] && body.Position.X == start.X && body.Position.Y == start.Y))
                continue;
        }
        body.ForwardPosition(transform);
        transform.RebuildAffine();
    }
}

void PhysicsSystem::Step()
{
    ++m_Stats.Steps;
    for (auto& entry : m_RigidBodies)
        entry.second.IsIntersecting = false;

    auto broadStart = std::chrono::steady_clock::now();
    FindPairs();
    auto narrowStart = std::chrono::steady_clock::now();
    NarrowPhase();
    auto contactsStart = std::chrono::steady_clock::now();
    CollectContacts();
    auto solveStart = std::chrono::steady_clock::now();
    Solve(SUB_STEP_SECONDS);
    auto end = std::chrono::steady_clock::now();

    auto ms = [](auto from, auto to) {
        return std::chrono::duration<double, std::milli>(to - from).count();
    };
    m_Stats.BroadMs += ms(broadStart, narrowStart);
    m_Stats.NarrowMs += ms(narrowStart, contactsStart);
    m_Stats.OtherMs += ms(contactsStart, solveStart);
    m_Stats.SolveMs += ms(solveStart, end);
}

void PhysicsSystem::FindPairs()
{
    // Every mode gives the same pairs, sorted by body index: the order the
    // all pairs loop visits them in, so contacts are solved in the same order
    // and the simulation gives exactly the same results
    const std::size_t count = m_RigidBodies.size();
    BroadPhase mode = m_BroadPhase;
    if (mode == BroadPhase::Automatic)
        mode = count >= GRID_BODIES ? BroadPhase::Grid : BroadPhase::Sweep;
    const bool sweep = mode == BroadPhase::Sweep;
    if (sweep)
        SortSweepOrder();

    // The boxes copied (in sweep order for the sweep) into one compact array:
    // the inner loops then read memory in order instead of jumping between
    // bodies
    m_SweepBoxes.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        std::uint32_t index = sweep ? m_SweepOrder[i] : static_cast<std::uint32_t>(i);
        const RigidBody& body = m_RigidBodies[index].second;
        SweepBox& box = m_SweepBoxes[i];
        box.Key = sweep ? m_SweepKeys[index] : 0.0f;
        box.MinX = body.RigidBodyAABB.Min.X;
        box.MaxX = body.RigidBodyAABB.Max.X;
        box.MinY = body.RigidBodyAABB.Min.Y;
        box.MaxY = body.RigidBodyAABB.Max.Y;
        box.Index = index;
        box.Static = body.IsStatic();
    }

    m_Pairs.clear();
    if (mode == BroadPhase::Grid)
        GridPairs();
    else if (mode == BroadPhase::AllPairs)
        AllPairs();
    else
        SweepPairs();
    std::sort(m_Pairs.begin(), m_Pairs.end());
}

void PhysicsSystem::SortSweepOrder()
{
    // Bodies sorted by the left edge of their bounding box
    const std::size_t count = m_RigidBodies.size();
    m_SweepKeys.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        float key = m_RigidBodies[i].second.RigidBodyAABB.Min.X;
        // A broken (NaN) box sorts last instead of breaking the sort
        m_SweepKeys[i] = std::isnan(key) ? std::numeric_limits<float>::infinity() : key;
    }
    auto before = [&](std::uint32_t a, std::uint32_t b) {
        return m_SweepKeys[a] < m_SweepKeys[b] || (m_SweepKeys[a] == m_SweepKeys[b] && a < b);
    };
    if (m_SweepOrder.size() == count)
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
        return;
    }
    m_SweepOrder.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        m_SweepOrder[i] = static_cast<std::uint32_t>(i);
    std::sort(m_SweepOrder.begin(), m_SweepOrder.end(), before);
}

void PhysicsSystem::SweepPairs()
{
    // Sort and sweep along X: each body is only compared with the bodies
    // that start before its right edge (instead of with every other body).
    // Runs on the engine's worker threads when there is enough work: each
    // chunk writes only its own slots, and the results are used in body
    // order, so the simulation is the same with any number of threads
    const std::size_t count = m_SweepBoxes.size();
    const std::size_t chunks = count >= PARALLEL_BODIES ? CHUNKS : 1;
    m_ChunkPairs.resize(chunks);
    std::vector<long long> tested(chunks, 0);
    RunChunks(chunks, [&](std::size_t chunk) {
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
                if (a.Overlaps(b))
                    pairs.push_back(OrderedPair(a.Index, b.Index));
            }
        }
        tested[chunk] = tests;
    });
    for (std::size_t chunk = 0; chunk < chunks; ++chunk)
    {
        m_Pairs.insert(m_Pairs.end(), m_ChunkPairs[chunk].begin(), m_ChunkPairs[chunk].end());
        m_Stats.PairsTested += tested[chunk];
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
    std::size_t moving = 0;
    for (const SweepBox& box : m_SweepBoxes)
    {
        if (!box.IsFinite())
            continue;
        minX = std::min(minX, box.MinX);
        minY = std::min(minY, box.MinY);
        maxX = std::max(maxX, box.MaxX);
        maxY = std::max(maxY, box.MaxY);
        if (!box.Static)
        {
            extent += std::max(box.MaxX - box.MinX, box.MaxY - box.MinY);
            ++moving;
        }
    }
    if (!(minX <= maxX))
        return;
    // Cells about twice the size of a typical moving body, at most a few
    // cells per body
    float cell = moving > 0 ? static_cast<float>(2.0 * extent / static_cast<double>(moving)) : 1.0f;
    cell = std::max(cell, 1e-3f);
    const float width = std::max(maxX - minX, cell);
    const float height = std::max(maxY - minY, cell);
    const double maxCells = 4.0 * static_cast<double>(count) + 64.0;
    while (static_cast<double>(std::ceil(width / cell)) *
                   static_cast<double>(std::ceil(height / cell)) >
           maxCells)
        cell *= 1.5f;
    const int columns = static_cast<int>(std::ceil(width / cell)) + 1;
    const int rows = static_cast<int>(std::ceil(height / cell)) + 1;
    auto column = [&](float x) {
        return std::clamp(static_cast<int>(std::floor((x - minX) / cell)), 0, columns - 1);
    };
    auto row = [&](float y) {
        return std::clamp(static_cast<int>(std::floor((y - minY) / cell)), 0, rows - 1);
    };
    auto cellIndex = [&](int r, int c) { return static_cast<std::size_t>(r) * columns + c; };

    // Count, then fill (one flat array, no per cell vectors)
    const std::size_t cells = static_cast<std::size_t>(columns) * static_cast<std::size_t>(rows);
    m_CellStart.assign(cells + 1, 0);
    m_BigBodies.clear();
    for (std::size_t i = 0; i < count; ++i)
    {
        const SweepBox& box = m_SweepBoxes[i];
        if (!box.IsFinite())
            continue;
        const int c0 = column(box.MinX), c1 = column(box.MaxX);
        const int r0 = row(box.MinY), r1 = row(box.MaxY);
        if ((c1 - c0 + 1) * (r1 - r0 + 1) > BIG_BODY_CELLS)
        {
            m_BigBodies.push_back(static_cast<std::uint32_t>(i));
            continue;
        }
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                ++m_CellStart[cellIndex(r, c) + 1];
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
        if (!box.IsFinite())
            continue;
        const int c0 = column(box.MinX), c1 = column(box.MaxX);
        const int r0 = row(box.MinY), r1 = row(box.MaxY);
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                m_CellBodies[m_CellFill[cellIndex(r, c)]++] = static_cast<std::uint32_t>(i);
    }

    for (int r = 0; r < rows; ++r)
    {
        for (int c = 0; c < columns; ++c)
        {
            const std::size_t first = m_CellStart[cellIndex(r, c)];
            const std::size_t last = m_CellStart[cellIndex(r, c) + 1];
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
                    if (!a.Overlaps(b))
                        continue;
                    // Only in the cell of the overlap's corner
                    if (column(std::max(a.MinX, b.MinX)) != c || row(std::max(a.MinY, b.MinY)) != r)
                        continue;
                    m_Pairs.push_back(OrderedPair(a.Index, b.Index));
                }
            }
        }
    }
    // Big bodies against every body (once per pair of big bodies)
    for (std::uint32_t bigIndex : m_BigBodies)
    {
        const SweepBox& a = m_SweepBoxes[bigIndex];
        for (std::size_t i = 0; i < count; ++i)
        {
            if (i == bigIndex)
                continue;
            if (i < bigIndex && std::binary_search(m_BigBodies.begin(),
                                                   m_BigBodies.end(),
                                                   static_cast<std::uint32_t>(i)))
                continue;
            const SweepBox& b = m_SweepBoxes[i];
            if (a.Static && b.Static)
                continue;
            ++m_Stats.PairsTested;
            if (a.Overlaps(b))
                m_Pairs.push_back(OrderedPair(a.Index, b.Index));
        }
    }
}

void PhysicsSystem::AllPairs()
{
    // The original all pairs test (reference for the tests)
    const std::size_t count = m_RigidBodies.size();
    for (std::uint32_t a = 0; a < count; ++a)
    {
        const RigidBody& ra = m_RigidBodies[a].second;
        for (std::uint32_t b = a + 1; b < count; ++b)
        {
            const RigidBody& rb = m_RigidBodies[b].second;
            if (ra.IsStatic() && rb.IsStatic())
                continue;
            ++m_Stats.PairsTested;
            if (AABBTest(ra.RigidBodyAABB, rb.RigidBodyAABB))
                m_Pairs.emplace_back(a, b);
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
    Concurrent::ForEach(
            m_ChunkIds.begin(), m_ChunkIds.end(), [&](std::size_t chunk) { work(chunk); });
}

void PhysicsSystem::NarrowPhase()
{
    // The exact test of every candidate pair (reads the bodies only), each
    // into its own slot, on the worker threads when there are enough pairs
    const std::size_t pairCount = m_Pairs.size();
    m_Narrow.clear();
    m_Narrow.resize(pairCount);
    const std::size_t chunks = pairCount >= PARALLEL_PAIRS ? CHUNKS : 1;
    RunChunks(chunks, [&](std::size_t chunk) {
        const std::size_t first = pairCount * chunk / chunks;
        const std::size_t last = pairCount * (chunk + 1) / chunks;
        for (std::size_t k = first; k < last; ++k)
        {
            auto& a = m_RigidBodies[m_Pairs[k].first];
            auto& b = m_RigidBodies[m_Pairs[k].second];
            m_Narrow[k].emplace(a.first, b.first, a.second, b.second);
        }
    });
}

void PhysicsSystem::CollectContacts()
{
    // In body order
    m_Collisions.clear();
    for (std::optional<Manifold>& narrow : m_Narrow)
    {
        Manifold& m = *narrow;
        if (!m.Collided)
            continue;
        m.A.IsIntersecting = true;
        m.B.IsIntersecting = true;
        ++m_Stats.Contacts;
        m_CallbackSystem->SubmitContact(m.EntityA, m.EntityB);
        if (m_CallbackSystem->HasRegisterCallback({m.A.Category, m.B.Category}))
            m_CallbackSystem->SubmitForCallback(m.EntityA, m.EntityB);
        m_Collisions.push_back(std::move(m));
    }
}

void PhysicsSystem::Solve(float deltaTime)
{
    // Kinetic friction with the ground plane
    for (auto& entry : m_RigidBodies)
    {
        RigidBody& body = entry.second;
        if (body.IsStatic() || !body.Collidable)
            continue;
        Vec2 frictionForce(0.0f, 0.0f);
        if (body.Velocity.GetMagnitudeSquared() > 0)
        {
            Vec2 direction = body.Velocity * -1.0f;
            direction.Normalize();
            frictionForce = direction * NORMAL_FORCE * body.DynamicFriction;
        }
        body.Velocity += frictionForce * deltaTime;
    }

    for (Manifold& manifold : m_Collisions)
        manifold.ResolveCollisionAngular();

    for (auto& entry : m_RigidBodies)
        entry.second.IntegrateVelocityAngular(deltaTime);

    for (Manifold& manifold : m_Collisions)
        manifold.PositionCorrection();

    for (auto& entry : m_RigidBodies)
    {
        RigidBody& body = entry.second;
        body.Force = Vec2(0.0f, 0.0f);
        body.RecomputeGeometry();
    }
}
