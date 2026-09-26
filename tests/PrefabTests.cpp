//---------------------------------------------------------------------------------
// PrefabTests.cpp
//---------------------------------------------------------------------------------
//
// Prefabs (World/Prefab.h): capturing a group of objects, placing copies,
// binary / text files and their validation, and scripts spawning prefabs
//
#include "AppStub.h"
#include "Mesh.h"
#include "RigidBody.h"
#include "Reflection/ComponentCatalog.h"
#include "Scripts/Components/GameComponents.h"
#include "World/Prefab.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using SceneObjects::BodyType;

namespace
{
    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    bool Near(const Vec3& a, const Vec3& b, float eps = 1e-3f)
    {
        return Near(a.X, b.X, eps) && Near(a.Y, b.Y, eps) && Near(a.Z, b.Z, eps);
    }

    std::string TempDir()
    {
        std::string dir = (std::filesystem::temp_directory_path() / "ubisoft_next_prefabs").string();
        std::filesystem::create_directories(dir);
        return dir;
    }

    struct Tower
    {
        Entity Root, Base, Turret, Crate, Outside;
    };

    // Root empty (turned 30 degrees) > Base (static shape, Health, Waypoint
    // to an object outside) > Turret (circle, script, Waypoint to Base);
    // Root > Crate (model)
    Tower BuildTower()
    {
        Fixture::FreshWorld();
        Tower t;
        t.Outside = SceneObjects::CreateShape(Fixture::ShapeOf("Outside", Shape2DType::Circle, {-8, 0, -8}));
        t.Root = SceneObjects::CreateEmpty("Tower", {5, 0, 5}, 30.0f);
        ECS.GetComponent<SceneObject>(t.Root).Tag = "Enemy";
        t.Base = SceneObjects::CreateShape(Fixture::ShapeOf("Base", Shape2DType::Rectangle, {6, 0, 5}, BodyType::Static));
        SceneObjects::SetParent(t.Base, t.Root);
        SceneObjects::ShapeDesc turret = Fixture::ShapeOf("Turret", Shape2DType::Circle, {6, 0.25f, 6});
        turret.Script = "Rotator";
        turret.ScriptParams = {{"Speed", 45.0f}};
        turret.Shape.Color = {0.1f, 0.2f, 0.3f};
        t.Turret = SceneObjects::CreateShape(turret);
        SceneObjects::SetParent(t.Turret, t.Base);
        t.Crate = SceneObjects::CreateModel("Crate", "Box", {4, 0, 4}, 0.0f, 0.5f);
        SceneObjects::SetParent(t.Crate, t.Root);
        Health health;
        health.Current = 42.0f;
        ECS.AddComponent<Health>(t.Base, health);
        ECS.AddComponent<Waypoint>(t.Base, Waypoint{t.Outside, 1.5f});
        ECS.AddComponent<Waypoint>(t.Turret, Waypoint{t.Base, 0.0f});
        return t;
    }

    Entity ChildNamed(Entity parent, const std::string& prefix)
    {
        for (Entity c : SceneObjects::GetChildren(parent))
        {
            if (ECS.GetComponent<SceneObject>(c).Name.rfind(prefix, 0) == 0)
                return c;
        }
        return NULL_ENTITY;
    }

    // Relative pose of b in a's frame (x, z) and relative yaw
    Vec3 Offset(Entity a, Entity b)
    {
        Transform ta = ECS.GetComponent<Transform>(a).GetWorldTransform();
        Vec3 d = SceneObjects::GetPosition(b) - ta.LocalPosition;
        return ta.LocalRotation.Inverse().RotatePoint(d);
    }
} // namespace

TEST_CASE("Prefabs: a captured group is placed again with the same layout and data")
{
    Tower t = BuildTower();
    Vec3 baseOffset = Offset(t.Root, t.Base);
    Vec3 turretOffset = Offset(t.Base, t.Turret);
    Prefab::Data prefab = Prefab::Capture(t.Root, "tower");
    REQUIRE(prefab.Objects.size() == 4u);
    CHECK_EQ(prefab.Name, std::string("tower"));
    CHECK_EQ(prefab.Objects[0].Parent, -1);
    CHECK_EQ(prefab.Objects[0].Name, std::string("Tower"));
    CHECK(prefab.Objects[0].Type == Prefab::ObjectType::Empty);
    // The root's place is not stored, only its height / rotation / scale
    CHECK(Near(prefab.Objects[0].Position, Vec3(0, 0, 0)));
    // Capturing leaves the scene untouched
    CHECK_EQ(ECS.GetComponent<Waypoint>(t.Turret).Next, t.Base);
    CHECK_EQ(ECS.GetComponent<Waypoint>(t.Base).Next, t.Outside);

    Entity copy = Prefab::Instantiate(prefab, {-4, 0, 2}, 90.0f);
    REQUIRE(copy != NULL_ENTITY);
    CHECK(Near(SceneObjects::GetPosition(copy), Vec3(-4, 0, 2)));
    CHECK(Near(SceneObjects::GetYaw(copy), 120.0f, 0.05f));
    CHECK_EQ(ECS.GetComponent<SceneObject>(copy).Name, std::string("Tower 2"));
    CHECK_EQ(ECS.GetComponent<SceneObject>(copy).Tag, std::string("Enemy"));
    REQUIRE(ECS.HasComponent<PrefabLink>(copy));
    CHECK_EQ(ECS.GetComponent<PrefabLink>(copy).Prefab, std::string("tower"));
    std::vector<Entity> objects = Prefab::InstanceObjects(copy);
    REQUIRE(objects.size() == 4u);

    Entity base = ChildNamed(copy, "Base");
    Entity crate = ChildNamed(copy, "Crate");
    REQUIRE(base != NULL_ENTITY);
    REQUIRE(crate != NULL_ENTITY);
    Entity turret = ChildNamed(base, "Turret");
    REQUIRE(turret != NULL_ENTITY);
    // Same layout relative to the parents
    CHECK(Near(Offset(copy, base), baseOffset));
    CHECK(Near(Offset(base, turret), turretOffset));
    // What each object is and carries
    CHECK(SceneObjects::GetBodyType(base) == BodyType::Static);
    CHECK_EQ(ECS.GetComponent<Mesh>(crate).Model, std::string("Box"));
    CHECK(Near(ECS.GetComponent<Transform>(crate).GetWorldTransform().LocalScale.X, 0.5f));
    CHECK(Fixture::Same(ECS.GetComponent<Shape2D>(turret).Color, Vec3(0.1f, 0.2f, 0.3f)));
    CHECK_EQ(ECS.GetComponent<ScriptComponent>(turret).Script, std::string("Rotator"));
    CHECK_EQ(ECS.GetComponent<ScriptComponent>(turret).Params.at("Speed"), 45.0f);
    CHECK_EQ(ECS.GetComponent<Health>(base).Current, 42.0f);
    // References inside the group point at the copies, outside ones are cleared
    CHECK_EQ(ECS.GetComponent<Waypoint>(turret).Next, base);
    CHECK_EQ(ECS.GetComponent<Waypoint>(base).Next, NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Waypoint>(base).WaitSeconds, 1.5f);

    // Under a parent
    Entity nested = Prefab::Instantiate(prefab, {0, 0, 0}, 0.0f, t.Outside);
    CHECK_EQ(SceneObjects::GetParent(nested), t.Outside);
    CHECK(Near(SceneObjects::GetPosition(nested), Vec3(0, 0, 0)));
    // Everything renders and simulates
    TestEnvironment::RunFrame(16.0f);
    CHECK(Near(ECS.GetComponent<RigidBody>(base).Position.X, SceneObjects::GetPosition(base).X));
}

TEST_CASE("Prefabs: binary and text files give back the same prefab")
{
    Tower t = BuildTower();
    Prefab::Data prefab = Prefab::Capture(t.Root, "tower");
    std::vector<std::uint8_t> binary = Prefab::Save(prefab, Serialization::SaveFormat::Binary);
    std::vector<std::uint8_t> text = Prefab::Save(prefab, Serialization::SaveFormat::Text);
    std::string asText(text.begin(), text.end());
    CHECK(asText.rfind("UBPF-TEXT 2\n", 0) == 0);
    CHECK(asText.find("prefab \"tower\" 4") != std::string::npos);
    CHECK(asText.find("object \"Turret\"") != std::string::npos);

    for (const auto* bytes : {&binary, &text})
    {
        Prefab::Data loaded;
        std::string error;
        std::vector<std::string> warnings;
        REQUIRE(Prefab::Load(*bytes, loaded, error, &warnings));
        CHECK(warnings.empty());
        // Saved again: exactly the same bytes
        CHECK(Prefab::Save(loaded, Serialization::SaveFormat::Binary) == binary);
    }

    // Files, names, the folder listing
    std::string dir = TempDir();
    std::string path = Prefab::PathOf("tower", dir);
    std::string error;
    REQUIRE(Prefab::SaveFile(path, prefab, error));
    CHECK(Prefab::Available(dir) == std::vector<std::string>{"tower"});
    Prefab::Data fromFile;
    REQUIRE(Prefab::LoadFile(path, fromFile, error));
    CHECK_EQ(fromFile.Objects.size(), size_t(4));
    CHECK_EQ(Prefab::SafeName("my tower/2"), std::string("my_tower_2"));
    std::filesystem::remove_all(dir);
}

TEST_CASE("Prefabs: damaged files are refused, unknown parts are warnings")
{
    Tower t = BuildTower();
    Prefab::Data prefab = Prefab::Capture(t.Root, "tower");
    std::vector<std::uint8_t> binary = Prefab::Save(prefab, Serialization::SaveFormat::Binary);
    Prefab::Data out;
    std::string error;

    auto refused = [&](std::vector<std::uint8_t> bytes, const std::string& why) {
        error.clear();
        CHECK(!Prefab::Load(bytes, out, error));
        CHECK(error.find(why) != std::string::npos);
    };
    std::vector<std::uint8_t> bad = binary;
    bad[0] = 'X';
    refused(bad, "not a prefab");
    bad = binary;
    bad.back() ^= 0xff;
    refused(bad, "checksum");
    bad = binary;
    bad.resize(bad.size() - 3);
    refused(bad, "truncated");
    bad = binary;
    bad[4] = 9; // format version
    refused(bad, "newer");
    refused({}, "");

    // Text edits: a child whose parent comes later, a broken component
    std::vector<std::uint8_t> textBytes = Prefab::Save(prefab, Serialization::SaveFormat::Text);
    std::string text(textBytes.begin(), textBytes.end());
    std::string loop = text;
    std::size_t at = loop.find("object \"Base\" \"\" 0 ");
    REQUIRE(at != std::string::npos);
    loop.replace(at, 19, "object \"Base\" \"\" 3 ");
    refused(std::vector<std::uint8_t>(loop.begin(), loop.end()), "parent must be an earlier object");

    Prefab::Data damaged = prefab;
    damaged.Objects[1].Components[0].Data.resize(3);
    refused(Prefab::Save(damaged, Serialization::SaveFormat::Binary), "component Health");

    // Unknown script / component: loaded with warnings, the component is skipped
    Prefab::Data unknown = prefab;
    unknown.Objects[2].Script = "NoSuchScript";
    unknown.Objects[2].Components.push_back({"NoSuchComponent", {1, 2, 3}});
    std::vector<std::string> warnings;
    REQUIRE(Prefab::Load(Prefab::Save(unknown, Serialization::SaveFormat::Binary), out, error, &warnings));
    CHECK_EQ(warnings.size(), size_t(2));
    std::vector<std::string> placeWarnings;
    Entity copy = Prefab::Instantiate(out, {0, 0, 0}, 0.0f, NULL_ENTITY, &placeWarnings);
    CHECK(copy != NULL_ENTITY);
    CHECK_EQ(placeWarnings.size(), size_t(1));
}

TEST_CASE("Prefabs: scripts spawn prefabs by name")
{
    // data/prefabs/turret.ubprefab is written by author_scenes
    Fixture::FreshWorld();
    Prefab::ClearCache();
    const Prefab::Data* turret = Prefab::Find("turret");
    REQUIRE(turret != nullptr);
    CHECK(Prefab::Find("turret") == turret);
    CHECK(Prefab::Find("no_such_prefab") == nullptr);
    std::size_t before = ECS.GetLivingEntities().size();

    struct Spawner : Script
    {
        Entity Spawned = NULL_ENTITY;
        void OnStart() override { Spawned = SpawnPrefab("turret", {3, 0, -2}, 45.0f); }
    } spawner;
    spawner.Bind("Spawner", SceneObjects::CreateEmpty("Spawner", {0, 0, 0}), {});
    spawner.OnStart();
    REQUIRE(spawner.Spawned != NULL_ENTITY);
    CHECK(Near(SceneObjects::GetPosition(spawner.Spawned), Vec3(3, 0, -2)));
    CHECK_EQ(ECS.GetComponent<PrefabLink>(spawner.Spawned).Prefab, std::string("turret"));
    CHECK_EQ(ECS.GetLivingEntities().size(), before + 1 + turret->Objects.size());
}
