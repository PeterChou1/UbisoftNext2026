//---------------------------------------------------------------------------------
// ComponentTests.cpp
//---------------------------------------------------------------------------------
//
// Reflected components (Reflection/Reflection.h, ComponentCatalog.h):
// field descriptions, generic serialization by field name (binary and text,
// with fields added / removed / reordered / retyped), the editor's add /
// remove / field editing with undo, scene files, and the example components
// with their scripts
//
#include "AppStub.h"
#include "Reflection/ComponentCatalog.h"
#include "SceneEditor.h"
#include "Scripts/Components/ComponentScripts.h"
#include "Scripts/Components/GameComponents.h"
#include "Serialization/TextArchive.h"
#include "WorldFixture.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>

using namespace Serialization;
using Reflection::FieldType;
using Reflection::FieldValue;
using Reflection::TypeInfoOf;

//-----------------------------------------------------------------------------
// Test types: every supported field type, and versions of one struct
//-----------------------------------------------------------------------------

enum class Mood
{
    Calm,
    Angry,
    Sleepy
};

struct Everything
{
    bool Flag = false;
    std::int8_t Small = 0;
    std::int16_t Medium = 0;
    int Count = 0;
    std::uint32_t Id = 0;
    float Speed = 0.0f;
    double Precise = 0.0;
    std::string Text;
    Vec2 Point = {0.0f, 0.0f};
    Vec3 Offset = {0.0f, 0.0f, 0.0f};
    Mood Feeling = Mood::Calm;
};

REFLECT(Everything)
{
    Field("Flag", &Everything::Flag);
    Field("Small", &Everything::Small);
    Field("Medium", &Everything::Medium);
    Field("Count", &Everything::Count).Range(-100, 100);
    Field("Id", &Everything::Id).AsEntity();
    Field("Speed", &Everything::Speed).Min(0).Step(0.25);
    Field("Precise", &Everything::Precise);
    Field("Text", &Everything::Text);
    Field("Point", &Everything::Point);
    Field("Offset", &Everything::Offset).Range(-1, 1);
    Field("Feeling", &Everything::Feeling).Options({"Calm", "Angry", "Sleepy"});
}

// Version 1 of a component...
struct EnemyV1
{
    float Speed = 1.0f;
    int Damage = 5;
    std::string Name = "grunt";
    bool Boss = false;
};

REFLECT(EnemyV1)
{
    Field("Speed", &EnemyV1::Speed);
    Field("Damage", &EnemyV1::Damage);
    Field("Name", &EnemyV1::Name);
    Field("Boss", &EnemyV1::Boss);
}

// ...and version 2: reordered, Damage is now a float, Name became a number,
// Boss was removed and Armor added
struct EnemyV2
{
    float Damage = 0.0f;
    double Speed = 0.0;
    float Name = 42.0f;
    int Armor = 3;
};

REFLECT(EnemyV2)
{
    Field("Damage", &EnemyV2::Damage);
    Field("Speed", &EnemyV2::Speed);
    Field("Name", &EnemyV2::Name);
    Field("Armor", &EnemyV2::Armor);
}

struct BadName
{
    int A = 0;
};

REFLECT(BadName)
{
    Field("Two words", &BadName::A);
}

struct Duplicated
{
    int A = 0;
    int B = 0;
};

REFLECT(Duplicated)
{
    Field("A", &Duplicated::A);
    Field("A", &Duplicated::B);
}

struct UnnamedEnum
{
    Mood Feeling = Mood::Calm;
};

REFLECT(UnnamedEnum)
{
    Field("Feeling", &UnnamedEnum::Feeling);
}

namespace
{
    template <typename T>
    std::string ToText(T value)
    {
        TextOutputArchive ar;
        ar(value);
        return ar.Text();
    }

    template <typename T>
    T FromText(const std::string& text)
    {
        T value{};
        TextInputArchive ar(text, 1);
        ar(value);
        REQUIRE(ar.AtEnd());
        return value;
    }

    template <typename From, typename To>
    To Convert(From value)
    {
        std::vector<std::uint8_t> bytes = ToBytes(value);
        To result{};
        FromBytes(bytes, result);
        return result;
    }

    Everything Filled()
    {
        Everything e;
        e.Flag = true;
        e.Small = -7;
        e.Medium = 3000;
        e.Count = -42;
        e.Id = 4000000000u;
        e.Speed = 0.1f;
        e.Precise = 1.0 / 3.0;
        e.Text = "a \"quoted\" name";
        e.Point = {1.5f, -2.0f};
        e.Offset = {0.25f, -0.5f, 1.0f};
        e.Feeling = Mood::Sleepy;
        return e;
    }

    void CheckSame(const Everything& a, const Everything& b)
    {
        CHECK_EQ(a.Flag, b.Flag);
        CHECK_EQ(a.Small, b.Small);
        CHECK_EQ(a.Medium, b.Medium);
        CHECK_EQ(a.Count, b.Count);
        CHECK_EQ(a.Id, b.Id);
        CHECK_EQ(a.Speed, b.Speed);
        CHECK_EQ(a.Precise, b.Precise);
        CHECK_EQ(a.Text, b.Text);
        CHECK(a.Point == b.Point);
        CHECK(a.Offset == b.Offset);
        CHECK(a.Feeling == b.Feeling);
    }

    const Reflection::FieldInfo& FieldOf(const Reflection::TypeInfo& info, const std::string& name)
    {
        const Reflection::FieldInfo* field = info.Find(name);
        REQUIRE(field != nullptr);
        return *field;
    }

    // Editor on a fresh world holding just the field
    Editor::SceneEditor& NewEditor()
    {
        static Editor::SceneEditor editor;
        Fixture::FreshWorld();
        editor = Editor::SceneEditor{};
        editor.NewScene();
        return editor;
    }

    Entity Box(Editor::SceneEditor& editor, const Vec3& position = {0, 0, 0})
    {
        return editor.Place(Editor::ObjectKind::Rectangle, position);
    }
} // namespace

//-----------------------------------------------------------------------------
// Field descriptions
//-----------------------------------------------------------------------------

TEST_CASE("Reflection: fields are described in declaration order with their editor hints")
{
    const Reflection::TypeInfo& info = TypeInfoOf<Everything>();
    CHECK_EQ(info.Name, std::string("Everything"));
    REQUIRE(info.Fields.size() == 11u);
    const char* names[] = {"Flag", "Small", "Medium", "Count", "Id", "Speed",
                           "Precise", "Text", "Point", "Offset", "Feeling"};
    FieldType types[] = {FieldType::Bool,   FieldType::Int,    FieldType::Int,  FieldType::Int,
                         FieldType::Entity, FieldType::Float,  FieldType::Float, FieldType::String,
                         FieldType::Vec2,   FieldType::Vec3,   FieldType::Enum};
    for (std::size_t i = 0; i < info.Fields.size(); ++i)
    {
        CHECK_EQ(info.Fields[i].Name, std::string(names[i]));
        CHECK(info.Fields[i].Type == types[i]);
    }
    CHECK_EQ(FieldOf(info, "Count").Min, -100.0);
    CHECK_EQ(FieldOf(info, "Count").Max, 100.0);
    CHECK_EQ(FieldOf(info, "Speed").StepOrDefault(), 0.25);
    CHECK_EQ(FieldOf(info, "Count").StepOrDefault(), 1.0);
    CHECK_EQ(FieldOf(info, "Precise").StepOrDefault(), 0.1);
    // Options without SERIALIZATION_ENUM_RANGE: values 0 .. n-1
    CHECK_EQ(FieldOf(info, "Feeling").EnumMin, std::int64_t(0));
    CHECK_EQ(FieldOf(info, "Feeling").EnumMax, std::int64_t(2));
    CHECK_EQ(FieldOf(info, "Feeling").OptionName(1), std::string("Angry"));
    CHECK(info.Find("Missing") == nullptr);

    // The example components
    const Reflection::TypeInfo& faction = TypeInfoOf<Faction>();
    CHECK(FieldOf(faction, "Banner").Type == FieldType::Color);
    CHECK(FieldOf(faction, "Kills").ReadOnly);
    // SERIALIZATION_ENUM_RANGE gives the enum's range
    CHECK_EQ(FieldOf(faction, "Side").EnumMax, std::int64_t(2));
    CHECK_EQ(FieldOf(TypeInfoOf<Health>(), "Invulnerable").Label, std::string("Invuln."));
    CHECK(FieldOf(TypeInfoOf<Waypoint>(), "Next").Type == FieldType::Entity);
    CHECK_EQ(FieldOf(TypeInfoOf<Waypoint>(), "Next").Tooltip, std::string("Object to go to next"));
}

TEST_CASE("Reflection: mistakes in a description are reported")
{
    CHECK_THROWS_AS(TypeInfoOf<BadName>(), std::logic_error);
    CHECK_THROWS_AS(TypeInfoOf<Duplicated>(), std::logic_error);
    CHECK_THROWS_AS(TypeInfoOf<UnnamedEnum>(), std::logic_error);
    // Hints that do not fit the field's type
    Reflection::FieldInfo number;
    number.Name = "N";
    number.Tag = Reflection::ValueTag::Float;
    CHECK_THROWS_AS(Reflection::FieldBuilder(number).AsColor(), std::logic_error);
    CHECK_THROWS_AS(Reflection::FieldBuilder(number).AsEntity(), std::logic_error);
    CHECK_THROWS_AS(Reflection::FieldBuilder(number).Options({"A"}), std::logic_error);
}

TEST_CASE("Reflection: values are converted and kept inside their range")
{
    const Reflection::TypeInfo& info = TypeInfoOf<Everything>();
    Everything e;
    auto set = [&](const char* name, const FieldValue& value) { return info.Set(&e, FieldOf(info, name), value); };

    // Numbers convert between integers and floating point (rounded)
    CHECK(set("Count", 7.6));
    CHECK_EQ(e.Count, 8);
    CHECK(set("Speed", std::int64_t(3)));
    CHECK_EQ(e.Speed, 3.0f);
    // Clamped to the declared range and to the C++ type
    CHECK(set("Count", 1000.0));
    CHECK_EQ(e.Count, 100);
    CHECK(set("Speed", -5.0));
    CHECK_EQ(e.Speed, 0.0f);
    CHECK(set("Small", std::int64_t(1000)));
    CHECK_EQ(e.Small, std::int8_t(127));
    CHECK(set("Offset", FieldValue(Vec3(5.0f, -0.5f, -9.0f))));
    CHECK(e.Offset == Vec3(1.0f, -0.5f, -1.0f));
    // Enums only take existing values
    CHECK(set("Feeling", std::int64_t(2)));
    CHECK(e.Feeling == Mood::Sleepy);
    CHECK(!set("Feeling", std::int64_t(3)));
    CHECK(!set("Feeling", std::int64_t(-1)));
    CHECK(e.Feeling == Mood::Sleepy);
    // Other types only take the same type
    CHECK(!set("Text", 1.0));
    CHECK(!set("Flag", std::int64_t(1)));
    CHECK(!set("Count", std::string("12")));
    CHECK(!set("Point", FieldValue(Vec3(1, 2, 3))));
    CHECK(!set("Speed", std::numeric_limits<double>::quiet_NaN()));
    CHECK(set("Text", std::string("hello")));
    CHECK_EQ(e.Text, std::string("hello"));

    // Get returns what Set stored
    CHECK(info.Get(&e, FieldOf(info, "Count")) == FieldValue(std::int64_t(100)));
    CHECK_EQ(Reflection::ToString(info.Get(&e, FieldOf(info, "Offset"))), std::string("(1, -0.5, -1)"));

    // Read only fields are shown, not set
    Faction faction;
    const Reflection::TypeInfo& factionInfo = TypeInfoOf<Faction>();
    CHECK(!factionInfo.Set(&faction, FieldOf(factionInfo, "Kills"), std::int64_t(5)));
    CHECK_EQ(faction.Kills, 0);
}

//-----------------------------------------------------------------------------
// Generic serialization
//-----------------------------------------------------------------------------

TEST_CASE("Reflection: any reflected struct is saved and loaded without a Serialize function")
{
    Everything original = Filled();
    // Binary
    CheckSame(Convert<Everything, Everything>(original), original);
    // Text, readable: [count] then Name:type value
    std::string text = ToText(original);
    CHECK_EQ(text, std::string("[11] Flag:b true Small:i -7 Medium:i 3000 Count:i -42 Id:u 4000000000 "
                               "Speed:f 0.1 Precise:d 0.3333333333333333 Text:s \"a \\\"quoted\\\" name\" "
                               "Point:v2 1.5 -2 Offset:v3 0.25 -0.5 1 Feeling:e 2"));
    CheckSame(FromText<Everything>(text), original);
    CHECK_EQ(ToText(Health{}), std::string("[4] Current:f 100 Max:f 100 Invulnerable:b false DestroyAtZero:b true"));
}

TEST_CASE("Reflection: fields are matched by name, like Unity")
{
    EnemyV1 old;
    old.Speed = 2.5f;
    old.Damage = 12;
    old.Name = "brute";
    old.Boss = true;

    auto check = [](const EnemyV2& loaded) {
        // Reordered and retyped numbers keep their value
        CHECK_EQ(loaded.Damage, 12.0f);
        CHECK_EQ(loaded.Speed, 2.5);
        // A value that no longer fits (string -> float) is ignored
        CHECK_EQ(loaded.Name, 42.0f);
        // New field: default value; removed field (Boss): skipped
        CHECK_EQ(loaded.Armor, 3);
    };
    check(Convert<EnemyV1, EnemyV2>(old));
    check(FromText<EnemyV2>(ToText(old)));

    // Hand written text: any order, missing fields keep their default
    EnemyV2 edited = FromText<EnemyV2>("[2] Armor:i 9 Damage:f 1.5");
    CHECK_EQ(edited.Armor, 9);
    CHECK_EQ(edited.Damage, 1.5f);
    CHECK_EQ(edited.Speed, 0.0);
    // And back: the values of unknown fields are skipped by their type
    EnemyV1 back = FromText<EnemyV1>("[3] Extra:v3 1 2 3 Name:s \"x\" Other:s \"y\"");
    CHECK_EQ(back.Name, std::string("x"));
    CHECK_EQ(back.Speed, 1.0f);
}

TEST_CASE("Reflection: damaged data is refused")
{
    // Unknown type tags
    CHECK_THROWS_AS(FromText<Health>("[1] Current:q 5"), SerializationError);
    CHECK_THROWS_AS(FromText<Health>("[1] Current 5"), SerializationError);
    CHECK_THROWS_AS(FromText<Health>("[1] :f 5"), SerializationError);
    // A value that does not match its tag
    CHECK_THROWS_AS(FromText<Health>("[1] Current:f true"), SerializationError);
    CHECK_THROWS_AS(FromText<Health>("[2] Current:f 5"), SerializationError);

    std::vector<std::uint8_t> bytes = ToBytes(*std::make_unique<Health>());
    // First field header: u32 count, u32 name length, "Current", u8 tag
    std::size_t tagAt = 4 + 4 + 7;
    REQUIRE(bytes[tagAt] == static_cast<std::uint8_t>(Reflection::ValueTag::Float));
    bytes[tagAt] = 99;
    Health h;
    CHECK_THROWS_AS(FromBytes(bytes, h), SerializationError);
    bytes.resize(tagAt);
    CHECK_THROWS_AS(FromBytes(bytes, h), SerializationError);
}

//-----------------------------------------------------------------------------
// Registration and scene files
//-----------------------------------------------------------------------------

TEST_CASE("Components: the catalog registers each component once, for the editor and scene files")
{
    Fixture::FreshWorld();
    ComponentCatalog& catalog = ComponentCatalog::Get();
    std::vector<std::string> names = catalog.Names();
    CHECK(names.size() >= 3u);
    CHECK(catalog.Find("Health") != nullptr);
    CHECK(catalog.Find("Health")->Type == &TypeInfoOf<Health>());
    CHECK(GetSceneSerializationRegistry().FindComponent("Health") != nullptr);
    CHECK(GetSceneSerializationRegistry().FindComponent("Waypoint") != nullptr);

    // Again: nothing changes. Another type under the same name, or the same
    // type under another name, is a mistake
    RegisterGameComponents();
    CHECK(catalog.Names() == names);
    CHECK_THROWS_AS(catalog.Register<Faction>("Health"), std::logic_error);
    CHECK_THROWS_AS(catalog.Register<Health>("Health2"), std::logic_error);
    CHECK(catalog.Names() == names);

    // Every component type still fits in the ECS signatures
    for (const ComponentEntry& entry : catalog.Entries())
    {
        Entity e = ECS.CreateEntity();
        entry.Add(ECS, e);
        CHECK(entry.Has(ECS, e));
        entry.Remove(ECS, e);
        CHECK(!entry.Has(ECS, e));
        ECS.DestroyEntity(e);
    }
    ECS.FlushECS();
}

TEST_CASE("Components: reflected components are saved in scene files, binary and text")
{
    Fixture::BuildSampleWorld();
    Entity a = SceneObjects::FindByName("Crate");
    Entity b = SceneObjects::FindByName("Player");
    REQUIRE(a != NULL_ENTITY);
    REQUIRE(b != NULL_ENTITY);
    Health health;
    health.Current = 35.5f;
    health.Invulnerable = true;
    ECS.AddComponent<Health>(a, health);
    Faction faction;
    faction.Side = Team::Enemy;
    faction.Title = "Red \"horde\"";
    faction.Banner = {0.9f, 0.3f, 0.25f};
    faction.Kills = 4;
    ECS.AddComponent<Faction>(a, faction);
    ECS.AddComponent<Waypoint>(b, Waypoint{a, 2.0f});
    Fixture::WorldImage before = Fixture::Capture();

    SaveMetadata meta = {{"Scene", "Play"}};
    std::vector<std::uint8_t> binary = Fixture::Serializer().Save(ECS, meta);
    std::string text = Fixture::Serializer().SaveText(ECS, meta);
    CHECK(text.find("component \"Health\" 1 [1]") != std::string::npos);
    CHECK(text.find("Current:f 35.5 Max:f 100 Invulnerable:b true") != std::string::npos);
    CHECK(text.find("Title:s \"Red \\\"horde\\\"\"") != std::string::npos);
    std::vector<std::string> warnings;
    CHECK(Fixture::Serializer().TextToBinary(text, warnings) == binary);

    for (bool useText : {false, true})
    {
        Fixture::FreshWorld();
        LoadResult loaded = useText ? Fixture::Serializer().Load(ECS, std::vector<std::uint8_t>(text.begin(), text.end()))
                                    : Fixture::Serializer().Load(ECS, binary);
        REQUIRE(loaded.Success);
        CHECK_SAME_WORLD(before, Fixture::Capture());
        REQUIRE(ECS.HasComponent<Health>(a));
        CHECK_EQ(ECS.GetComponent<Health>(a).Current, 35.5f);
        CHECK(ECS.GetComponent<Health>(a).Invulnerable);
        const Faction& f = ECS.GetComponent<Faction>(a);
        CHECK(f.Side == Team::Enemy);
        CHECK_EQ(f.Title, faction.Title);
        CHECK(f.Banner == faction.Banner);
        CHECK_EQ(f.Kills, 4);
        CHECK_EQ(ECS.GetComponent<Waypoint>(b).Next, a);
        CHECK(!ECS.HasComponent<Health>(b));
    }
}

TEST_CASE("Components: files only list the component types they use")
{
    Fixture::BuildSampleWorld();
    std::string text = Fixture::Serializer().SaveText(ECS, {});
    CHECK(text.find("\"Health\"") == std::string::npos);
    CHECK(text.find("\"Transform\"") != std::string::npos);
    Fixture::WorldImage before = Fixture::Capture();

    // Files written before (with empty lists for unused types) still load
    std::size_t at = text.find("component \"Transform\"");
    REQUIRE(at != std::string::npos);
    text.insert(at, "component \"Health\" 1 [0]\ncomponent \"Emitter\" 1 [0]\n");
    Fixture::FreshWorld();
    LoadResult loaded = Fixture::Serializer().Load(ECS, std::vector<std::uint8_t>(text.begin(), text.end()));
    REQUIRE(loaded.Success);
    CHECK(loaded.Warnings.empty());
    CHECK_SAME_WORLD(before, Fixture::Capture());
}

TEST_CASE("Components: hand edited component records load by field name")
{
    Fixture::BuildSampleWorld();
    Entity crate = SceneObjects::FindByName("Crate");
    ECS.AddComponent<Health>(crate, Health{});
    std::string text = Fixture::Serializer().SaveText(ECS, {});
    std::string from = "[4] Current:f 100 Max:f 100 Invulnerable:b false DestroyAtZero:b true";
    std::size_t at = text.find(from);
    REQUIRE(at != std::string::npos);
    // Reordered, one field missing, one unknown
    text.replace(at, from.size(), "[3] Max:f 250 Shield:f 9 Current:i 80");
    Fixture::FreshWorld();
    LoadResult loaded = Fixture::Serializer().Load(ECS, std::vector<std::uint8_t>(text.begin(), text.end()));
    REQUIRE(loaded.Success);
    const Health& h = ECS.GetComponent<Health>(crate);
    CHECK_EQ(h.Max, 250.0f);
    CHECK_EQ(h.Current, 80.0f);
    CHECK(h.DestroyAtZero);
}

//-----------------------------------------------------------------------------
// Editor
//-----------------------------------------------------------------------------

TEST_CASE("Editor components: add and remove components, with undo")
{
    Editor::SceneEditor& editor = NewEditor();
    Entity e = Box(editor);
    std::vector<std::string> addable = editor.AddableComponents(e);
    REQUIRE(addable.size() >= 5u);
    CHECK_EQ(addable[0], std::string("RigidBody"));
    CHECK_EQ(addable[1], std::string("Script"));
    CHECK(std::find(addable.begin(), addable.end(), "Health") != addable.end());

    std::vector<Editor::ComponentView> views = editor.ComponentsOf(e);
    REQUIRE(views.size() == 3u);
    CHECK_EQ(views[0].Name, std::string("Transform"));
    CHECK(!views[0].Removable);

    std::size_t undo = editor.UndoCount();
    REQUIRE(editor.AddComponent(e, "Health"));
    CHECK(ECS.HasComponent<Health>(e));
    CHECK_EQ(ECS.GetComponent<Health>(e).Current, 100.0f);
    CHECK_EQ(editor.UndoCount(), undo + 1);
    CHECK(editor.IsDirty());
    // Once only
    CHECK(!editor.AddComponent(e, "Health"));
    views = editor.ComponentsOf(e);
    CHECK_EQ(views.back().Name, std::string("Health"));
    CHECK(views.back().Removable);
    CHECK(views.back().Type == &TypeInfoOf<Health>());

    // Built in components: a body (static) and a script
    REQUIRE(editor.AddComponent(e, "RigidBody"));
    CHECK(SceneObjects::GetBodyType(e) == SceneObjects::BodyType::Static);
    REQUIRE(editor.AddComponent(e, "Script"));
    CHECK(!editor.GetScript(e).empty());
    CHECK(editor.HasComponent(e, "Script"));
    REQUIRE(editor.RemoveComponent(e, "Script"));
    CHECK(editor.GetScript(e).empty());
    REQUIRE(editor.RemoveComponent(e, "RigidBody"));
    CHECK(SceneObjects::GetBodyType(e) == SceneObjects::BodyType::None);

    // Transform, SceneObject, Shape2D stay; unknown names are refused
    CHECK(!editor.RemoveComponent(e, "Transform"));
    CHECK(!editor.RemoveComponent(e, "Shape2D"));
    CHECK(!editor.AddComponent(e, "Nothing"));
    CHECK(!editor.RemoveComponent(e, "Faction"));

    REQUIRE(editor.RemoveComponent(e, "Health"));
    CHECK(!ECS.HasComponent<Health>(e));
    REQUIRE(editor.Undo());
    CHECK(ECS.HasComponent<Health>(e));
    REQUIRE(editor.Redo());
    CHECK(!ECS.HasComponent<Health>(e));

    // The field has no components to add
    Entity field = editor.Objects()[0];
    CHECK(editor.AddableComponents(field).empty());
    CHECK(!editor.AddComponent(field, "Health"));
}

TEST_CASE("Editor components: set fields by name, one undo step per change")
{
    Editor::SceneEditor& editor = NewEditor();
    Entity e = Box(editor);
    Entity target = Box(editor, {3, 0, 0});
    REQUIRE(editor.AddComponent(e, "Health"));
    REQUIRE(editor.AddComponent(e, "Faction"));
    REQUIRE(editor.AddComponent(e, "Waypoint"));

    std::size_t undo = editor.UndoCount();
    REQUIRE(editor.SetField(e, "Health", "Current", 40.0));
    CHECK_EQ(ECS.GetComponent<Health>(e).Current, 40.0f);
    CHECK_EQ(editor.UndoCount(), undo + 1);
    // The same value again is not a change
    REQUIRE(editor.SetField(e, "Health", "Current", std::int64_t(40)));
    CHECK_EQ(editor.UndoCount(), undo + 1);
    // Clamped to the range
    REQUIRE(editor.SetField(e, "Health", "Current", 1e9));
    CHECK_EQ(ECS.GetComponent<Health>(e).Current, 10000.0f);
    REQUIRE(editor.SetField(e, "Health", "Invulnerable", true));
    REQUIRE(editor.SetField(e, "Faction", "Side", std::int64_t(1)));
    CHECK(ECS.GetComponent<Faction>(e).Side == Team::Player);
    REQUIRE(editor.SetField(e, "Faction", "Title", std::string("Blue")));
    REQUIRE(editor.SetField(e, "Faction", "Banner", FieldValue(Vec3(0.3f, 0.5f, 0.9f))));
    REQUIRE(editor.SetField(e, "Waypoint", "Next", std::int64_t(target)));
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).Next, target);

    // Refused: read only, wrong type, missing field / component, not an object
    std::size_t before = editor.UndoCount();
    CHECK(!editor.SetField(e, "Faction", "Kills", std::int64_t(3)));
    CHECK(!editor.SetField(e, "Faction", "Side", std::int64_t(7)));
    CHECK(!editor.SetField(e, "Faction", "Title", 3.0));
    CHECK(!editor.SetField(e, "Health", "Nope", 1.0));
    CHECK(!editor.SetField(target, "Health", "Current", 1.0));
    CHECK(!editor.SetField(e, "Waypoint", "Next", std::int64_t(4000)));
    CHECK_EQ(editor.UndoCount(), before);

    FieldValue value;
    REQUIRE(editor.GetField(e, "Faction", "Title", value));
    CHECK(value == FieldValue(std::string("Blue")));
    CHECK(!editor.GetField(e, "Faction", "Nope", value));

    REQUIRE(editor.Undo());
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).Next, NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Faction>(e).Title, std::string("Blue"));
}

TEST_CASE("Editor components: duplicates copy them, removed objects are no longer referenced")
{
    Editor::SceneEditor& editor = NewEditor();
    Entity a = Box(editor);
    Entity b = Box(editor, {4, 0, 0});
    REQUIRE(editor.AddComponent(a, "Health"));
    REQUIRE(editor.SetField(a, "Health", "Max", 300.0));
    REQUIRE(editor.AddComponent(a, "Waypoint"));
    REQUIRE(editor.SetField(a, "Waypoint", "Next", std::int64_t(b)));

    Entity copy = editor.Duplicate(a);
    REQUIRE(copy != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Health>(copy).Max, 300.0f);
    CHECK_EQ(ECS.GetComponent<Waypoint>(copy).Next, b);

    REQUIRE(editor.Remove(b));
    CHECK_EQ(ECS.GetComponent<Waypoint>(a).Next, NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Waypoint>(copy).Next, NULL_ENTITY);
    REQUIRE(editor.Undo());
    CHECK_EQ(ECS.GetComponent<Waypoint>(a).Next, b);
}

TEST_CASE("Editor components: saved with the scene and loaded back")
{
    Editor::SceneEditor& editor = NewEditor();
    Entity e = Box(editor);
    REQUIRE(editor.AddComponent(e, "Faction"));
    REQUIRE(editor.SetField(e, "Faction", "Title", std::string("Knights")));
    REQUIRE(editor.SetField(e, "Faction", "Rank", 7.0));
    std::string path = (std::filesystem::temp_directory_path() / "ubisoft_next_components.ubsave").string();
    REQUIRE(editor.SaveScene(path, "components").Success);

    Editor::SceneEditor& other = NewEditor();
    REQUIRE(other.LoadScene(path).Success);
    Entity loaded = SceneObjects::FindByName(editor.NameOf(e));
    REQUIRE(loaded != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Faction>(loaded).Title, std::string("Knights"));
    CHECK_EQ(ECS.GetComponent<Faction>(loaded).Rank, 7);
    std::filesystem::remove(path);
}

//-----------------------------------------------------------------------------
// Scripts using the example components
//-----------------------------------------------------------------------------

TEST_CASE("Component scripts: WaypointFollower walks the path of Waypoint components")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    Entity p1 = SceneObjects::CreateShape(Fixture::ShapeOf("P1", Shape2DType::Circle, {4, 0, 0}));
    Entity p2 = SceneObjects::CreateShape(Fixture::ShapeOf("P2", Shape2DType::Circle, {4, 0, 4}));
    ECS.AddComponent<Waypoint>(p1, Waypoint{p2, 0.5f});
    ECS.AddComponent<Waypoint>(p2, Waypoint{p1, 0.0f});
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Walker", Shape2DType::Rectangle, {0, 0, 0});
    desc.Script = "WaypointFollower";
    desc.ScriptParams = {{"Speed", 2.0f}};
    Entity walker = SceneObjects::CreateShape(desc);
    ECS.AddComponent<Waypoint>(walker, Waypoint{p1, 0.0f});

    TestEnvironment::RunFrames(50, 20.0f); // 1 s: halfway to P1
    CHECK(std::fabs(SceneObjects::GetPosition(walker).X - 2.0f) < 0.1f);
    TestEnvironment::RunFrames(90, 20.0f); // at P1 (2 s), waits 0.5 s, then 0.3 s towards P2
    Vec3 p = SceneObjects::GetPosition(walker);
    CHECK(std::fabs(p.X - 4.0f) < 0.01f);
    CHECK(p.Z > 0.1f && p.Z < 1.0f);
    auto* script = dynamic_cast<WaypointFollower*>(GameSceneManager.Scripts().GetScript(walker));
    REQUIRE(script != nullptr);
    CHECK_EQ(script->Target(), p2);
}

TEST_CASE("Component scripts: DamageZone removes Health and destroys at zero")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    SceneObjects::ShapeDesc zoneDesc = Fixture::ShapeOf("Zone", Shape2DType::Rectangle, {0, 0, 0});
    zoneDesc.Script = "DamageZone";
    zoneDesc.ScriptParams = {{"Damage", 40.0f}};
    Entity zone = SceneObjects::CreateShape(zoneDesc);
    Entity target = SceneObjects::CreateShape(Fixture::ShapeOf("Target", Shape2DType::Circle, {5, 0, 0}));
    Entity shielded = SceneObjects::CreateShape(Fixture::ShapeOf("Shielded", Shape2DType::Circle, {-5, 0, 0}));
    ECS.AddComponent<Health>(target, Health{});
    Health invulnerable;
    invulnerable.Invulnerable = true;
    ECS.AddComponent<Health>(shielded, invulnerable);
    TestEnvironment::RunFrame(20.0f);
    auto* script = dynamic_cast<DamageZone*>(GameSceneManager.Scripts().GetScript(zone));
    REQUIRE(script != nullptr);

    script->Hit(target);
    CHECK_EQ(ECS.GetComponent<Health>(target).Current, 60.0f);
    script->Hit(shielded);
    CHECK_EQ(ECS.GetComponent<Health>(shielded).Current, 100.0f);
    script->Hit(zone); // no Health: nothing happens
    script->Hit(target);
    CHECK_EQ(ECS.GetComponent<Health>(target).Current, 20.0f);
    script->Hit(target);
    TestEnvironment::RunFrame(20.0f);
    CHECK(!ECS.IsEntityAlive(target));

    // Kept at 0 without DestroyAtZero
    Health lasting;
    lasting.Current = 10.0f;
    lasting.DestroyAtZero = false;
    Entity survivor = SceneObjects::CreateShape(Fixture::ShapeOf("Survivor", Shape2DType::Circle, {0, 0, 5}));
    ECS.AddComponent<Health>(survivor, lasting);
    script->Hit(survivor);
    TestEnvironment::RunFrame(20.0f);
    REQUIRE(ECS.IsEntityAlive(survivor));
    CHECK_EQ(ECS.GetComponent<Health>(survivor).Current, 0.0f);
}

TEST_CASE("Component scripts: the sandbox scene's walker goes round its path")
{
    std::vector<std::uint8_t> bytes;
    std::string error;
    REQUIRE(WorldSerializer::ReadFile(GameManager::ScenePath("sandbox"), bytes, error));
    Fixture::FreshWorld();
    AppStub::Reset();
    REQUIRE(Fixture::Serializer().Load(ECS, bytes).Success);
    Entity walker = SceneObjects::FindByName("Walker");
    Entity first = SceneObjects::FindByName("Waypoint_1");
    std::vector<Entity> players = SceneObjects::FindByTag("Player");
    REQUIRE(players.size() == 1u);
    Entity player = players[0];
    REQUIRE(walker != NULL_ENTITY);
    REQUIRE(first != NULL_ENTITY);
    REQUIRE(player != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Waypoint>(walker).Next, first);
    CHECK_EQ(ECS.GetComponent<Faction>(walker).Title, std::string("Patrol"));
    CHECK(ECS.GetComponent<Faction>(player).Side == Team::Player);
    CHECK(!ECS.GetComponent<Health>(player).DestroyAtZero);
    // Three waypoints in a loop
    Entity point = first;
    for (int i = 0; i < 3; ++i)
        point = ECS.GetComponent<Waypoint>(point).Next;
    CHECK_EQ(point, first);

    TestEnvironment::RunFrames(100, 20.0f);
    Vec3 p = SceneObjects::GetPosition(walker);
    CHECK(!(p == Vec3(-12, p.Y, -8)));
}
