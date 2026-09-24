//---------------------------------------------------------------------------------
// WorldSerializationTests.cpp
//---------------------------------------------------------------------------------
//
// End to end tests: save a complete game state, change / clear the world,
// load it back and verify the resulting game state is identical
//
#include "GameWorldFixture.h"
#include "Serialization/Crc32.h"

#include <cstdio>
#include <filesystem>

using namespace Serialization;
using Fixture::Capture;
using Fixture::WorldImage;

namespace
{
    WorldSerializer Serializer() { return WorldSerializer(Fixture::Registry()); }

    std::string TempPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / ("ubisoft_next_tests_" + name)).string();
    }

    /**
     * \brief Plays a few "frames" of gameplay by hand: units move and take
     *        damage, an enemy dies, a crystal is mined, a new unit spawns and
     *        the round advances. Used to show that loading rewinds everything
     */
    void PlaySomeRounds(const Fixture::SampleLevel& level)
    {
        for (Entity soldier : level.Soldiers)
        {
            auto& unit = ECS.GetComponent<PlayerControlUnit>(soldier);
            unit.health -= 30;
            unit.selected = !unit.selected;
            auto& t = ECS.GetComponent<Transform>(soldier);
            t.SetLocalPosition(t.LocalPosition + Vec3(1.0f, 0.0f, 0.5f));
            ECS.GetComponent<RigidBody>(soldier).Velocity = Vec2(0, 0);
        }
        ECS.GetComponent<PlayerBaseComponent>(level.Base).PlayerBaseHealth = 12;
        ECS.GetComponent<CrystalDeposit>(level.Crystals[0]).AmountOfCrystal -= 5;
        ECS.DestroyEntity(level.Enemies[0]);
        ECS.DestroyEntity(level.Bullet);
        ECS.RemoveComponent<UITarget>(level.Selector);
        Entity reinforcement = Fixture::MeshEntity({0, 0, 3}, SoldierUnitAsset);
        ECS.AddComponent<PlayerControlUnit>(reinforcement, {4, 100, false});

        auto state = ECS.GetResource<GameState>();
        state->RoundNumber = 5;
        state->currentState = Spawn;
        state->PlayerCrystalInventory = 0;
        state->ObstacleInCursor = NULL_ENTITY;

        auto board = ECS.GetResource<BlackBoard>();
        board->PlayerTankTargets.clear();
        board->LastKnownLocation[reinforcement] = Vec3(1, 1, 1);

        // Wall got placed, back to the default interaction mode
        ECS.GetResource<UIState>()->state = DefaultContext;
        ECS.GetResource<UIState>()->flipped = false;
        ECS.FlushECS();
    }
} // namespace

TEST_CASE("World: a full game state survives save -> clear -> load")
{
    Fixture::BuildSampleLevel();
    WorldImage before = Capture(ECS);
    std::vector<std::uint8_t> save = Serializer().Save(ECS, {{"Scene", "MainLevel"}});

    Fixture::FreshWorld();
    REQUIRE(ECS.GetEntityCount() == 0);

    LoadResult result = Serializer().Load(ECS, save);
    REQUIRE(result.Success);
    CHECK(result.Warnings.empty());
    CHECK_EQ(result.Metadata["Scene"], std::string("MainLevel"));

    WorldImage after = Capture(ECS);
    CHECK_SAME_WORLD(before, after);
    CHECK_EQ(ECS.GetEntityCount(), before.Living.size());
}

TEST_CASE("World: loading rewinds gameplay progress made after the save")
{
    Fixture::SampleLevel level = Fixture::BuildSampleLevel();
    WorldImage saved = Capture(ECS);
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    PlaySomeRounds(level);
    WorldImage played = Capture(ECS);
    REQUIRE(!Fixture::Diff(saved, played).empty());

    LoadResult result = Serializer().Load(ECS, save);
    REQUIRE(result.Success);
    CHECK_SAME_WORLD(saved, Capture(ECS));

    // Spot checks of individual gameplay values
    auto state = ECS.GetResource<GameState>();
    CHECK_EQ(state->RoundNumber, 4);
    CHECK(state->currentState == Invasion);
    CHECK_EQ(state->PlayerCrystalInventory, 42);
    CHECK_EQ(state->ObstacleInCursor, level.WallInCursor);
    CHECK_EQ(ECS.GetComponent<PlayerBaseComponent>(level.Base).PlayerBaseHealth, 730);
    CHECK_EQ(ECS.GetComponent<PlayerControlUnit>(level.Soldiers[1]).health, 85);
    CHECK(ECS.IsEntityAlive(level.Enemies[0]));
    CHECK(ECS.IsEntityAlive(level.Bullet));
    CHECK(ECS.HasComponent<UITarget>(level.Selector));
    CHECK_EQ(ECS.GetComponent<CrystalDeposit>(level.Crystals[0]).AmountOfCrystal, 30);
    // Still placing the (already paid for) wall, with the same orientation
    CHECK(ECS.GetResource<UIState>()->state == BuildObstacleContext);
    CHECK(ECS.GetResource<UIState>()->flipped);
}

TEST_CASE("World: entity ids, parent/child links and cross references are preserved")
{
    Fixture::SampleLevel level = Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);

    Transform& tank = ECS.GetComponent<Transform>(level.PlayerTank);
    CHECK(tank.Children ==
          std::vector<Entity>({level.PlayerTankBase, level.PlayerTankCannon}));
    CHECK_EQ(ECS.GetComponent<Transform>(level.PlayerTankCannon).Parent, level.PlayerTank);
    CHECK(ECS.GetComponent<Mesh>(level.PlayerTankCannon).MeshType == CannonTank);
    CHECK(ECS.GetComponent<PlayerControlUnit>(level.PlayerTank).isTank);

    auto board = ECS.GetResource<BlackBoard>();
    CHECK_EQ(board->UnitTarget, level.Selector);
    CHECK_EQ(board->PlayerTankTargets[level.PlayerTankCannon], level.Enemies[0]);
    CHECK_EQ(board->EnemyTankTargets[level.EnemyTankCannon], level.Soldiers[0]);
}

TEST_CASE("World: destroyed entities stay destroyed and id allocation continues identically")
{
    Fixture::SampleLevel level = Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    // Ids the original session would hand out next
    std::vector<Entity> expectedNext;
    for (int i = 0; i < 5; ++i)
        expectedNext.push_back(ECS.CreateEntity());

    Fixture::FreshWorld();
    // Allocate some entities so the allocator is in a different state
    for (int i = 0; i < 100; ++i)
        ECS.CreateEntity();

    REQUIRE(Serializer().Load(ECS, save).Success);
    for (Entity dead : level.Destroyed)
    {
        CHECK(!ECS.IsEntityAlive(dead));
        CHECK(!ECS.HasComponent<Transform>(dead));
    }

    std::vector<Entity> actualNext;
    for (int i = 0; i < 5; ++i)
        actualNext.push_back(ECS.CreateEntity());
    CHECK(actualNext == expectedNext);
}

TEST_CASE("World: visitors (systems) see the restored entities")
{
    Fixture::SampleLevel level = Fixture::BuildSampleLevel();
    std::set<Entity> units = ECS.Visit<PlayerControlUnit, RigidBody>();
    std::set<Entity> enemies = ECS.Visit<BasicEnemyUnit>();
    std::set<Entity> meshes = ECS.Visit<Transform, Mesh>();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);

    CHECK(ECS.Visit<PlayerControlUnit, RigidBody>() == units);
    CHECK(ECS.Visit<BasicEnemyUnit>() == enemies);
    CHECK(ECS.Visit<Transform, Mesh>() == meshes);
    CHECK(ECS.Visit<Transform, TankBullet>() == std::set<Entity>({level.Bullet}));
    // Nothing is reported as deleted to systems after a load
    CHECK(ECS.VisitDeleted<BasicEnemyUnit>().empty());
    CHECK(ECS.VisitDeleted<Transform, Mesh>().empty());
}

TEST_CASE("World: loaded render components are marked for re-initialization")
{
    Fixture::SampleLevel level = Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);

    for (Entity e : ECS.Visit<Mesh>())
        CHECK(!ECS.GetComponent<Mesh>(e).Loaded);
    for (Entity e : ECS.Visit<FragShaderTag>())
    {
        CHECK(!ECS.GetComponent<FragShaderTag>(e).Initialized);
        CHECK_EQ(ECS.GetComponent<FragShaderTag>(e).FragShaderID, size_t(0));
    }
    CHECK(!ECS.GetComponent<Particle>(level.Particle).loaded);
    CHECK(ECS.GetComponent<FragShaderTag>(level.WallInCursor).FragAssetId == RedShaderID);
}

TEST_CASE("World: save -> load -> save produces identical bytes")
{
    Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> first = Serializer().Save(ECS, {{"Scene", "MainLevel"}});
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, first).Success);
    std::vector<std::uint8_t> second = Serializer().Save(ECS, {{"Scene", "MainLevel"}});
    CHECK(first == second);
}

TEST_CASE("World: empty world round trip")
{
    Fixture::FreshWorld();
    WorldImage before = Capture(ECS);
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::BuildSampleLevel();
    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK_SAME_WORLD(before, Capture(ECS));
    CHECK_EQ(ECS.GetEntityCount(), size_t(0));
}

TEST_CASE("World: entities without any component are preserved")
{
    Fixture::FreshWorld();
    Entity a = ECS.CreateEntity();
    Entity b = ECS.CreateEntity();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK(ECS.IsEntityAlive(a));
    CHECK(ECS.IsEntityAlive(b));
    CHECK_EQ(ECS.GetEntityCount(), size_t(2));
}

TEST_CASE("World: large battle with thousands of entities")
{
    Fixture::FreshWorld();
    for (int i = 0; i < 4000; ++i)
    {
        Entity e = Fixture::MeshEntity({float(i % 100), 0, float(i / 100)},
                                       i % 2 ? SoldierUnitAsset : BasicEnemy);
        RigidBody body(0.3f, 0.3f);
        body.Velocity = Vec2(0.001f * i, -0.001f * i);
        ECS.AddComponent<RigidBody>(e, body);
        if (i % 2)
            ECS.AddComponent<PlayerControlUnit>(e, {i % 7, 100, false});
        else
            ECS.AddComponent<BasicEnemyUnit>(e, {100, 0.001f});
    }
    // Kill every third unit
    for (Entity e = 3; e < 4000; e += 3)
        ECS.DestroyEntity(e);
    ECS.FlushECS();

    WorldImage before = Capture(ECS);
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK_SAME_WORLD(before, Capture(ECS));
}

//-----------------------------------------------------------------------------
// Robustness: bad files never corrupt the running game
//-----------------------------------------------------------------------------

TEST_CASE("Robustness: a corrupted save is rejected and the world is left untouched")
{
    Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    Fixture::SampleLevel current = Fixture::BuildSampleLevel();
    PlaySomeRounds(current);
    WorldImage beforeLoad = Capture(ECS);

    // Flip one bit at a handful of positions across the file
    for (size_t pos : {size_t(5), size_t(20), save.size() / 3, save.size() / 2, save.size() - 6})
    {
        std::vector<std::uint8_t> corrupt = save;
        corrupt[pos] ^= 0x10;
        LoadResult result = Serializer().Load(ECS, corrupt);
        CHECK(!result.Success);
        CHECK(!result.Error.empty());
        CHECK_SAME_WORLD(beforeLoad, Capture(ECS));
    }
}

TEST_CASE("Robustness: silently changed gameplay values are caught by the checksum")
{
    // Tampering with a value (not the structure) keeps the file parseable,
    // only the checksum can detect it
    Fixture::BuildSampleLevel();
    ECS.GetResource<GameState>()->CurrentTimeInvasion = 13.37f;
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    float marker = 13.37f;
    std::uint8_t pattern[4];
    std::memcpy(pattern, &marker, 4);
    auto it = std::search(save.begin(), save.end(), pattern, pattern + 4);
    REQUIRE(it != save.end());
    *it ^= 0x01;

    Fixture::FreshWorld();
    LoadResult result = Serializer().Load(ECS, save);
    CHECK(!result.Success);
    CHECK(result.Error.find("checksum") != std::string::npos);
    CHECK_EQ(ECS.GetEntityCount(), size_t(0));
}

TEST_CASE("Robustness: a truncated save is rejected and the world is left untouched")
{
    Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    WorldImage beforeLoad = Capture(ECS);

    for (size_t size : {size_t(0), size_t(3), size_t(12), save.size() / 2, save.size() - 1})
    {
        std::vector<std::uint8_t> truncated(save.begin(), save.begin() + size);
        LoadResult result = Serializer().Load(ECS, truncated);
        CHECK(!result.Success);
    }
    CHECK_SAME_WORLD(beforeLoad, Capture(ECS));
}

TEST_CASE("Robustness: wrong magic and future format versions are rejected")
{
    Fixture::BuildSampleLevel();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    std::vector<std::uint8_t> badMagic = save;
    badMagic[0] = 'X';
    LoadResult magicResult = Serializer().Load(ECS, badMagic);
    CHECK(!magicResult.Success);
    CHECK(magicResult.Error.find("magic") != std::string::npos);

    // Re-stamp a future version with a valid checksum so only the version is wrong
    std::vector<std::uint8_t> future = save;
    future[4] = 99;
    future.resize(future.size() - 4);
    std::uint32_t crc = Crc32(future.data(), future.size());
    for (int i = 0; i < 4; ++i)
        future.push_back(static_cast<std::uint8_t>(crc >> (8 * i)));
    LoadResult versionResult = Serializer().Load(ECS, future);
    CHECK(!versionResult.Success);
    CHECK(versionResult.Error.find("version") != std::string::npos);
}

TEST_CASE("Robustness: random garbage never crashes the loader")
{
    Fixture::BuildSampleLevel();
    WorldImage beforeLoad = Capture(ECS);
    std::uint32_t seed = 12345;
    for (int attempt = 0; attempt < 200; ++attempt)
    {
        std::vector<std::uint8_t> garbage(16 + attempt * 7);
        for (auto& byte : garbage)
        {
            seed = seed * 1664525u + 1013904223u;
            byte = static_cast<std::uint8_t>(seed >> 24);
        }
        if (attempt % 2 == 0)
        {
            garbage[0] = 'U';
            garbage[1] = 'B';
            garbage[2] = 'S';
            garbage[3] = 'V';
        }
        CHECK(!Serializer().Load(ECS, garbage).Success);
    }
    CHECK_SAME_WORLD(beforeLoad, Capture(ECS));
}

//-----------------------------------------------------------------------------
// Compatibility between game versions
//-----------------------------------------------------------------------------

namespace
{
    struct Stamina
    {
        float Value = 0.0f;
    };

    template <typename Archive>
    void Serialize(Archive& ar, Stamina& s)
    {
        ar(s.Value);
    }

    // Version 1 of a component stored only health, version 2 added armor
    struct Armor
    {
        int Health = 0;
        int ArmorPoints = 0;
    };

    struct ArmorV1Layout
    {
        int Health = 0;
    };

    template <typename Archive>
    void Serialize(Archive& ar, ArmorV1Layout& a)
    {
        ar(a.Health);
    }

    template <typename Archive>
    void Serialize(Archive& ar, Armor& a)
    {
        ar(a.Health);
        if (ar.Version() >= 2)
            ar(a.ArmorPoints);
        else if constexpr (Archive::IsLoading)
            a.ArmorPoints = 10; // default for units saved before armor existed
    }
} // namespace

namespace
{
    // Simulates a programmer adding a field without bumping the version
    struct ProbeWriter : Resource
    {
        int A = 0;
        int B = 0;
        void ResetResource() override {}
    };

    struct ProbeReader : Resource
    {
        int A = 0;
        void ResetResource() override {}
    };

    template <typename Archive>
    void Serialize(Archive& ar, ProbeWriter& p)
    {
        ar(p.A, p.B);
    }

    template <typename Archive>
    void Serialize(Archive& ar, ProbeReader& p)
    {
        ar(p.A);
    }

    struct PositionWriter
    {
        float X = 0, Y = 0;
    };

    struct PositionReader
    {
        float X = 0;
    };

    template <typename Archive>
    void Serialize(Archive& ar, PositionWriter& p)
    {
        ar(p.X, p.Y);
    }

    template <typename Archive>
    void Serialize(Archive& ar, PositionReader& p)
    {
        ar(p.X);
    }

    void RegisterProbes()
    {
        static bool registered = false;
        if (!registered)
        {
            ECS.RegisterResource(ProbeWriter());
            ECS.RegisterResource(ProbeReader());
            registered = true;
        }
    }
} // namespace

TEST_CASE("Robustness: a resource layout mismatch aborts the load before touching the world")
{
    RegisterProbes();
    SerializationRegistry writer;
    writer.RegisterResource<ProbeWriter>("Probe");
    SerializationRegistry reader;
    reader.RegisterResource<ProbeReader>("Probe");

    Fixture::FreshWorld();
    ECS.GetResource<ProbeWriter>()->A = 5;
    ECS.GetResource<ProbeWriter>()->B = 6;
    std::vector<std::uint8_t> save = WorldSerializer(writer).Save(ECS);

    Fixture::BuildSampleLevel();
    ECS.GetResource<ProbeReader>()->A = 1;
    WorldImage before = Capture(ECS);
    LoadResult result = WorldSerializer(reader).Load(ECS, save);
    CHECK(!result.Success);
    CHECK(result.Error.find("Probe") != std::string::npos);
    CHECK_EQ(ECS.GetResource<ProbeReader>()->A, 1);
    CHECK_SAME_WORLD(before, Capture(ECS));
}

TEST_CASE("Robustness: a component layout mismatch aborts the load before touching the world")
{
    SerializationRegistry writer;
    writer.RegisterComponent<PositionWriter>("Position");
    SerializationRegistry reader;
    reader.RegisterComponent<PositionReader>("Position");

    Fixture::FreshWorld();
    Entity e = ECS.CreateEntity();
    ECS.AddComponent<PositionWriter>(e, {1.0f, 2.0f});
    std::vector<std::uint8_t> save = WorldSerializer(writer).Save(ECS);

    Fixture::BuildSampleLevel();
    WorldImage before = Capture(ECS);
    LoadResult result = WorldSerializer(reader).Load(ECS, save);
    CHECK(!result.Success);
    CHECK(result.Error.find("Position") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture(ECS));
}

TEST_CASE("Compatibility: unknown component types from a newer build are skipped")
{
    SerializationRegistry newer;
    RegisterEngineSerializers(newer);
    RegisterGameSerializers(newer);
    newer.RegisterComponent<Stamina>("Stamina");

    Fixture::SampleLevel level = Fixture::BuildSampleLevel();
    ECS.AddComponent<Stamina>(level.Soldiers[0], {0.5f});
    ECS.AddComponent<Stamina>(level.Soldiers[1], {0.75f});
    WorldImage before = Capture(ECS);
    std::vector<std::uint8_t> save = WorldSerializer(newer).Save(ECS);

    Fixture::FreshWorld();
    LoadResult result = Serializer().Load(ECS, save);
    REQUIRE(result.Success);
    REQUIRE(result.Warnings.size() == 1);
    CHECK(result.Warnings[0].find("Stamina") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture(ECS));
    CHECK(!ECS.HasComponent<Stamina>(level.Soldiers[0]));
}

TEST_CASE("Compatibility: component layout versions are migrated on load")
{
    SerializationRegistry v1;
    v1.RegisterComponent<ArmorV1Layout>("Armor", 1);
    SerializationRegistry v2;
    v2.RegisterComponent<Armor>("Armor", 2);

    Fixture::FreshWorld();
    Entity e = ECS.CreateEntity();
    ECS.AddComponent<ArmorV1Layout>(e, {70});
    std::vector<std::uint8_t> oldSave = WorldSerializer(v1).Save(ECS);

    Fixture::FreshWorld();
    REQUIRE(WorldSerializer(v2).Load(ECS, oldSave).Success);
    REQUIRE(ECS.HasComponent<Armor>(e));
    CHECK_EQ(ECS.GetComponent<Armor>(e).Health, 70);
    CHECK_EQ(ECS.GetComponent<Armor>(e).ArmorPoints, 10);

    // Current version round trip keeps the new field
    ECS.GetComponent<Armor>(e).ArmorPoints = 3;
    std::vector<std::uint8_t> newSave = WorldSerializer(v2).Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(WorldSerializer(v2).Load(ECS, newSave).Success);
    CHECK_EQ(ECS.GetComponent<Armor>(e).ArmorPoints, 3);

    // An old build can not read a newer layout and must refuse instead of guessing
    Fixture::FreshWorld();
    LoadResult result = WorldSerializer(v1).Load(ECS, newSave);
    CHECK(!result.Success);
    CHECK(result.Error.find("version") != std::string::npos);
}

TEST_CASE("Compatibility: registering a type or name twice is an error")
{
    SerializationRegistry registry;
    registry.RegisterComponent<Stamina>("Stamina");
    CHECK_THROWS_AS(registry.RegisterComponent<Stamina>("Stamina2"), SerializationError);
    CHECK_THROWS_AS(registry.RegisterComponent<Armor>("Stamina"), SerializationError);
    CHECK_THROWS_AS(registry.RegisterComponent<Armor>("Armor", 0), SerializationError);
}

TEST_CASE("Compatibility: post load callbacks run after the world is restored")
{
    SerializationRegistry registry;
    RegisterEngineSerializers(registry);
    RegisterGameSerializers(registry);
    size_t unitsSeen = 0;
    int roundSeen = 0;
    registry.AddPostLoadCallback([&](ECSManager& ecs) {
        unitsSeen = ecs.Visit<PlayerControlUnit>().size();
        roundSeen = ecs.GetResource<GameState>()->RoundNumber;
    });

    Fixture::BuildSampleLevel();
    size_t expectedUnits = ECS.Visit<PlayerControlUnit>().size();
    std::vector<std::uint8_t> save = WorldSerializer(registry).Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(WorldSerializer(registry).Load(ECS, save).Success);
    CHECK_EQ(unitsSeen, expectedUnits);
    CHECK_EQ(roundSeen, 4);
}

//-----------------------------------------------------------------------------
// Files
//-----------------------------------------------------------------------------

TEST_CASE("Files: save to disk and load back")
{
    std::string path = TempPath("dir/quicksave.ubsave");
    std::filesystem::remove_all(std::filesystem::path(path).parent_path());

    Fixture::BuildSampleLevel();
    WorldImage before = Capture(ECS);
    SaveResult saved = Serializer().SaveToFile(ECS, path, {{"Scene", "MainLevel"}});
    REQUIRE(saved.Success);
    CHECK(saved.BytesWritten > 0);
    CHECK_EQ(std::filesystem::file_size(path), saved.BytesWritten);
    CHECK(!std::filesystem::exists(path + ".tmp"));

    Fixture::FreshWorld();
    LoadResult loaded = Serializer().LoadFromFile(ECS, path);
    REQUIRE(loaded.Success);
    CHECK_EQ(loaded.Metadata["Scene"], std::string("MainLevel"));
    CHECK_SAME_WORLD(before, Capture(ECS));

    // Overwriting an existing save works
    ECS.GetResource<GameState>()->RoundNumber = 9;
    REQUIRE(Serializer().SaveToFile(ECS, path).Success);
    ECS.GetResource<GameState>()->RoundNumber = 1;
    REQUIRE(Serializer().LoadFromFile(ECS, path).Success);
    CHECK_EQ(ECS.GetResource<GameState>()->RoundNumber, 9);

    std::filesystem::remove_all(std::filesystem::path(path).parent_path());
}

TEST_CASE("Files: missing file reports an error without touching the world")
{
    Fixture::BuildSampleLevel();
    WorldImage before = Capture(ECS);
    LoadResult result = Serializer().LoadFromFile(ECS, TempPath("does_not_exist.ubsave"));
    CHECK(!result.Success);
    CHECK(result.Error.find("Could not open") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture(ECS));
}

TEST_CASE("Files: two-phase load exposes metadata before applying")
{
    Fixture::BuildSampleLevel();
    WorldImage saved = Capture(ECS);
    std::vector<std::uint8_t> save =
            Serializer().Save(ECS, {{"Scene", "MainLevel"}, {"Round", "4"}});

    Fixture::FreshWorld();
    WorldSnapshot snapshot;
    LoadResult parsed = Serializer().Parse(save, snapshot);
    REQUIRE(parsed.Success);
    CHECK_EQ(parsed.Metadata["Scene"], std::string("MainLevel"));
    CHECK_EQ(parsed.Metadata["Round"], std::string("4"));
    // Parsing alone does not modify the world
    CHECK_EQ(ECS.GetEntityCount(), size_t(0));

    std::vector<std::string> warnings = Serializer().Apply(ECS, snapshot);
    CHECK(warnings.empty());
    CHECK_SAME_WORLD(saved, Capture(ECS));
}
