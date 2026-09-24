//---------------------------------------------------------------------------------
// SerializationTests.cpp
//---------------------------------------------------------------------------------
//
// End to end tests of the scene / save format: save a world built from every
// scene feature, change or clear it, load it back and verify the world is
// identical (compared field by field, never with the serializer itself)
//
#include "Serialization/Crc32.h"
#include "WorldFixture.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <set>

using namespace Serialization;
using Fixture::Capture;
using Fixture::Serializer;
using Fixture::WorldImage;

namespace
{
    std::string TempPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / ("ubisoft_next_tests_" + name)).string();
    }

    /**
     * \brief What playing the scene does to the world: objects move and spin,
     *        a pickup is collected, a projectile is spawned, scene settings
     *        change. Used to show that loading rewinds everything
     */
    void PlayABit(const Fixture::SampleWorld& w)
    {
        SceneObjects::SetPosition(w.Player, {4, 0, 4});
        ECS.GetComponent<RigidBody>(w.Player).Velocity = Vec2(0, 0);
        SceneObjects::SetYaw(w.Spinner, 135.0f);
        SceneObjects::Destroy(w.Pickup);
        ECS.GetComponent<Shape2D>(w.Wall).Color = Vec3(1, 0, 0);
        ECS.GetComponent<ScriptComponent>(w.Player).Params["Speed"] = 1.0f;
        ECS.RemoveComponent<ScriptComponent>(w.Spinner);

        SceneObjects::ShapeDesc shot = Fixture::ShapeOf("Shot", Shape2DType::Circle, {0, 0, 0},
                                                        SceneObjects::BodyType::Trigger);
        shot.Script = "Projectile";
        SceneObjects::CreateShape(shot);

        auto settings = ECS.GetResource<SceneSettings>();
        settings->SceneParams["Lives"] = 0.0f;
        settings->CameraDistance = 99.0f;
        ECS.FlushECS();
    }
} // namespace

//-----------------------------------------------------------------------------
// World round trips
//-----------------------------------------------------------------------------

TEST_CASE("World: a full scene survives save -> clear -> load")
{
    Fixture::BuildSampleWorld();
    WorldImage before = Capture();
    std::vector<std::uint8_t> save = Serializer().Save(ECS, {{"Scene", "Play"}});

    Fixture::FreshWorld();
    REQUIRE(ECS.GetEntityCount() == 0);

    LoadResult result = Serializer().Load(ECS, save);
    REQUIRE(result.Success);
    CHECK(result.Warnings.empty());
    CHECK_EQ(result.Metadata["Scene"], std::string("Play"));

    CHECK_SAME_WORLD(before, Capture());
    CHECK_EQ(ECS.GetEntityCount(), before.Living.size());
}

TEST_CASE("World: loading rewinds everything that happened after the save")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    WorldImage saved = Capture();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    PlayABit(w);
    REQUIRE(!Fixture::Diff(saved, Capture()).empty());

    LoadResult result = Serializer().Load(ECS, save);
    REQUIRE(result.Success);
    CHECK_SAME_WORLD(saved, Capture());

    // Spot checks of individual values
    CHECK(ECS.IsEntityAlive(w.Pickup));
    CHECK_EQ(ECS.GetComponent<SceneObject>(w.Pickup).Tag, std::string("Pickup"));
    CHECK_EQ(ECS.GetComponent<ScriptComponent>(w.Player).Params.at("Speed"), 7.5f);
    CHECK_EQ(ECS.GetComponent<ScriptComponent>(w.Spinner).Script, std::string("Rotator"));
    CHECK_EQ(SceneObjects::GetYaw(w.Wall), 30.0f);
    CHECK_EQ(ECS.GetResource<SceneSettings>()->SceneParams.at("Lives"), 2.0f);
    CHECK(SceneObjects::FindByName("Shot") == NULL_ENTITY);
}

TEST_CASE("World: entity ids and parent/child links are preserved")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);

    CHECK(ECS.GetComponent<Transform>(w.Parent).Children == std::vector<Entity>({w.Child}));
    CHECK_EQ(ECS.GetComponent<Transform>(w.Child).Parent, w.Parent);
    CHECK_EQ(SceneObjects::FindByName("Player"), w.Player);
    CHECK_EQ(SceneObjects::FindByName("Crate"), w.Crate);
    CHECK_EQ(ECS.GetComponent<Mesh>(w.Crate).Model, std::string("Box"));
}

TEST_CASE("World: destroyed entities stay destroyed and id allocation continues identically")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
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
    for (Entity dead : w.Destroyed)
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
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    std::set<Entity> shapes = ECS.Visit<Transform, Shape2D>();
    std::set<Entity> bodies = ECS.Visit<RigidBody>();
    std::set<Entity> scripts = ECS.Visit<ScriptComponent>();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);

    CHECK(ECS.Visit<Transform, Shape2D>() == shapes);
    CHECK(ECS.Visit<RigidBody>() == bodies);
    CHECK(ECS.Visit<ScriptComponent>() == scripts);
    CHECK(ECS.Visit<Transform, Mesh>() == std::set<Entity>({w.Crate}));
    // Nothing is reported as deleted to systems after a load
    CHECK(ECS.VisitDeleted<Shape2D>().empty());
    CHECK(ECS.VisitDeleted<Transform, Mesh>().empty());
}

TEST_CASE("World: loaded render components are marked for rebuilding")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    // Pretend everything was already built by the render systems
    for (Entity e : ECS.Visit<Shape2D>())
        ECS.GetComponent<Shape2D>(e).Built = true;
    ECS.GetComponent<Mesh>(w.Crate).Loaded = true;
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);

    for (Entity e : ECS.Visit<Shape2D>())
        CHECK(!ECS.GetComponent<Shape2D>(e).Built);
    for (Entity e : ECS.Visit<Mesh>())
        CHECK(!ECS.GetComponent<Mesh>(e).Loaded);
    for (Entity e : ECS.Visit<FragShaderTag>())
    {
        CHECK(!ECS.GetComponent<FragShaderTag>(e).Initialized);
        CHECK_EQ(ECS.GetComponent<FragShaderTag>(e).FragShaderID, size_t(0));
    }
    CHECK(ECS.GetComponent<FragShaderTag>(w.Wall).FragAssetId == ShapeShaderID);
    CHECK(ECS.GetComponent<FragShaderTag>(w.Crate).FragAssetId == BlinnPhongID);
}

TEST_CASE("World: save -> load -> save produces identical bytes")
{
    Fixture::BuildSampleWorld();
    std::vector<std::uint8_t> first = Serializer().Save(ECS, {{"Scene", "Play"}});
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, first).Success);
    std::vector<std::uint8_t> second = Serializer().Save(ECS, {{"Scene", "Play"}});
    CHECK(first == second);
}

TEST_CASE("World: empty world round trip")
{
    Fixture::FreshWorld();
    WorldImage before = Capture();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::BuildSampleWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK_SAME_WORLD(before, Capture());
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

TEST_CASE("World: thousands of scene objects")
{
    Fixture::FreshWorld();
    for (int i = 0; i < 3000; ++i)
    {
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Object " + std::to_string(i),
                                                        static_cast<Shape2DType>(i % 4),
                                                        {float(i % 100) * 0.5f, 0, float(i / 100) * 0.5f},
                                                        static_cast<SceneObjects::BodyType>(i % 4));
        desc.Shape.Sides = 3 + i % 10;
        desc.YawDegrees = float(i % 360);
        if (i % 5 == 0)
        {
            desc.Script = "Rotator";
            desc.ScriptParams = {{"Speed", float(i)}};
        }
        SceneObjects::CreateShape(desc);
    }
    // Remove every third object
    for (Entity e = 2; e < 3000; e += 3)
        SceneObjects::Destroy(e);
    ECS.FlushECS();

    WorldImage before = Capture();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK_SAME_WORLD(before, Capture());
}

//-----------------------------------------------------------------------------
// Components
//-----------------------------------------------------------------------------

TEST_CASE("Component: every shape type and body type round trips")
{
    Fixture::FreshWorld();
    std::vector<Entity> created;
    for (int t = 0; t < static_cast<int>(Shape2DType::Count); ++t)
    {
        for (int b = 0; b < static_cast<int>(SceneObjects::BodyType::Count); ++b)
        {
            SceneObjects::ShapeDesc desc = Fixture::ShapeOf(
                    "S" + std::to_string(t) + std::to_string(b), static_cast<Shape2DType>(t),
                    {float(t * 3), 0, float(b * 3)}, static_cast<SceneObjects::BodyType>(b));
            desc.Shape.Width = 1.0f + 0.25f * t;
            desc.Shape.Height = 0.5f + 0.25f * b;
            desc.Shape.Sides = 5 + t;
            desc.Shape.Thickness = 0.1f * (b + 1);
            desc.Shape.Color = Vec3(0.1f * t, 0.2f * b, 0.3f);
            desc.YawDegrees = 15.0f * (t + b);
            created.push_back(SceneObjects::CreateShape(desc));
        }
    }
    WorldImage before = Capture();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK_SAME_WORLD(before, Capture());
    for (Entity e : created)
    {
        SceneObjects::BodyType expected = static_cast<SceneObjects::BodyType>(
                ECS.GetComponent<SceneObject>(e).Name.back() - '0');
        CHECK(SceneObjects::GetBodyType(e) == expected);
    }
}

TEST_CASE("Component: RigidBody keeps private mass / inertia / restitution")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    RigidBody original = ECS.GetComponent<RigidBody>(w.Player);
    REQUIRE(original.InvMass() > 0.0f);
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, save).Success);
    const RigidBody& loaded = ECS.GetComponent<RigidBody>(w.Player);
    CHECK(Fixture::Same(original.InvMass(), loaded.InvMass()));
    CHECK(Fixture::Same(original.InvInertia(), loaded.InvInertia()));
    CHECK(Fixture::Same(original.Restitution(), loaded.Restitution()));
    CHECK(ECS.GetComponent<RigidBody>(w.Wall).IsStatic());
    CHECK(!ECS.GetComponent<RigidBody>(w.Pickup).Collidable);
}

TEST_CASE("Resource: SceneSettings round trip and reset")
{
    Fixture::BuildSampleWorld();
    SceneSettings saved = *ECS.GetResource<SceneSettings>();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    ECS.GetResource<SceneSettings>()->ResetResource();
    CHECK(ECS.GetResource<SceneSettings>()->SceneScript.empty());
    CHECK(ECS.GetResource<SceneSettings>()->SceneParams.empty());

    REQUIRE(Serializer().Load(ECS, save).Success);
    CHECK(Fixture::Same(saved, *ECS.GetResource<SceneSettings>()));
}

//-----------------------------------------------------------------------------
// Robustness: bad files never corrupt the running game
//-----------------------------------------------------------------------------

TEST_CASE("Robustness: a corrupted save is rejected and the world is left untouched")
{
    Fixture::BuildSampleWorld();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);

    Fixture::SampleWorld current = Fixture::BuildSampleWorld();
    PlayABit(current);
    WorldImage beforeLoad = Capture();

    // Flip one bit at a handful of positions across the file
    for (size_t pos : {size_t(5), size_t(20), save.size() / 3, save.size() / 2, save.size() - 6})
    {
        std::vector<std::uint8_t> corrupt = save;
        corrupt[pos] ^= 0x10;
        LoadResult result = Serializer().Load(ECS, corrupt);
        CHECK(!result.Success);
        CHECK(!result.Error.empty());
        CHECK_SAME_WORLD(beforeLoad, Capture());
    }
}

TEST_CASE("Robustness: silently changed values are caught by the checksum")
{
    // Tampering with a value (not the structure) keeps the file parseable,
    // only the checksum can detect it
    Fixture::BuildSampleWorld();
    ECS.GetResource<SceneSettings>()->CameraDistance = 13.37f;
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
    Fixture::BuildSampleWorld();
    std::vector<std::uint8_t> save = Serializer().Save(ECS);
    WorldImage beforeLoad = Capture();

    for (size_t size : {size_t(0), size_t(3), size_t(12), save.size() / 2, save.size() - 1})
    {
        std::vector<std::uint8_t> truncated(save.begin(), save.begin() + size);
        LoadResult result = Serializer().Load(ECS, truncated);
        CHECK(!result.Success);
    }
    CHECK_SAME_WORLD(beforeLoad, Capture());
}

TEST_CASE("Robustness: wrong magic and future format versions are rejected")
{
    Fixture::BuildSampleWorld();
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
    Fixture::BuildSampleWorld();
    WorldImage beforeLoad = Capture();
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
    CHECK_SAME_WORLD(beforeLoad, Capture());
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

    Fixture::BuildSampleWorld();
    ECS.GetResource<ProbeReader>()->A = 1;
    WorldImage before = Capture();
    LoadResult result = WorldSerializer(reader).Load(ECS, save);
    CHECK(!result.Success);
    CHECK(result.Error.find("Probe") != std::string::npos);
    CHECK_EQ(ECS.GetResource<ProbeReader>()->A, 1);
    CHECK_SAME_WORLD(before, Capture());
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

    Fixture::BuildSampleWorld();
    WorldImage before = Capture();
    LoadResult result = WorldSerializer(reader).Load(ECS, save);
    CHECK(!result.Success);
    CHECK(result.Error.find("Position") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture());
}

TEST_CASE("Compatibility: unknown component types from a newer build are skipped")
{
    SerializationRegistry newer;
    RegisterEngineSerializers(newer);
    RegisterSceneSerializers(newer);
    newer.RegisterComponent<Stamina>("Stamina");

    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    ECS.AddComponent<Stamina>(w.Player, {0.5f});
    ECS.AddComponent<Stamina>(w.Crate, {0.75f});
    WorldImage before = Capture();
    std::vector<std::uint8_t> save = WorldSerializer(newer).Save(ECS);

    Fixture::FreshWorld();
    LoadResult result = Serializer().Load(ECS, save);
    REQUIRE(result.Success);
    REQUIRE(result.Warnings.size() == 1);
    CHECK(result.Warnings[0].find("Stamina") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture());
    CHECK(!ECS.HasComponent<Stamina>(w.Player));
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
    RegisterSceneSerializers(registry);
    size_t shapesSeen = 0;
    std::string scriptSeen;
    registry.AddPostLoadCallback([&](ECSManager& ecs) {
        shapesSeen = ecs.Visit<Shape2D>().size();
        scriptSeen = ecs.GetResource<SceneSettings>()->SceneScript;
    });

    Fixture::BuildSampleWorld();
    size_t expectedShapes = ECS.Visit<Shape2D>().size();
    std::vector<std::uint8_t> save = WorldSerializer(registry).Save(ECS);
    Fixture::FreshWorld();
    REQUIRE(WorldSerializer(registry).Load(ECS, save).Success);
    CHECK_EQ(shapesSeen, expectedShapes);
    CHECK_EQ(scriptSeen, std::string("CollectGame"));
}

//-----------------------------------------------------------------------------
// Files
//-----------------------------------------------------------------------------

TEST_CASE("Files: save to disk and load back")
{
    std::string path = TempPath("dir/quicksave.ubsave");
    std::filesystem::remove_all(std::filesystem::path(path).parent_path());

    Fixture::BuildSampleWorld();
    WorldImage before = Capture();
    SaveResult saved = Serializer().SaveToFile(ECS, path, {{"Scene", "Play"}});
    REQUIRE(saved.Success);
    CHECK(saved.BytesWritten > 0);
    CHECK_EQ(std::filesystem::file_size(path), saved.BytesWritten);
    CHECK(!std::filesystem::exists(path + ".tmp"));

    Fixture::FreshWorld();
    LoadResult loaded = Serializer().LoadFromFile(ECS, path);
    REQUIRE(loaded.Success);
    CHECK_EQ(loaded.Metadata["Scene"], std::string("Play"));
    CHECK_SAME_WORLD(before, Capture());

    // Overwriting an existing save works
    ECS.GetResource<SceneSettings>()->FieldWidth = 9.0f;
    REQUIRE(Serializer().SaveToFile(ECS, path).Success);
    ECS.GetResource<SceneSettings>()->FieldWidth = 1.0f;
    REQUIRE(Serializer().LoadFromFile(ECS, path).Success);
    CHECK_EQ(ECS.GetResource<SceneSettings>()->FieldWidth, 9.0f);

    std::filesystem::remove_all(std::filesystem::path(path).parent_path());
}

TEST_CASE("Files: missing file reports an error without touching the world")
{
    Fixture::BuildSampleWorld();
    WorldImage before = Capture();
    LoadResult result = Serializer().LoadFromFile(ECS, TempPath("does_not_exist.ubsave"));
    CHECK(!result.Success);
    CHECK(result.Error.find("Could not open") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture());
}

TEST_CASE("Files: two-phase load exposes metadata before applying")
{
    Fixture::BuildSampleWorld();
    WorldImage saved = Capture();
    std::vector<std::uint8_t> save = Serializer().Save(ECS, {{"Scene", "Play"}, {"Name", "sample"}});

    Fixture::FreshWorld();
    WorldSnapshot snapshot;
    LoadResult parsed = Serializer().Parse(save, snapshot);
    REQUIRE(parsed.Success);
    CHECK_EQ(parsed.Metadata["Scene"], std::string("Play"));
    CHECK_EQ(parsed.Metadata["Name"], std::string("sample"));
    // Parsing alone does not modify the world
    CHECK_EQ(ECS.GetEntityCount(), size_t(0));

    std::vector<std::string> warnings = Serializer().Apply(ECS, snapshot);
    CHECK(warnings.empty());
    CHECK_SAME_WORLD(saved, Capture());
}
