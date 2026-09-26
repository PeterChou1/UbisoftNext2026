#include "SampleScenes.h"

#include "Scripts/Components/GameComponents.h"
#include "Scripts/MetalInvasion/MINames.h"
#include "Scripts/MetalInvasion/MIPrefabs.h"
#include "Scripts/ScriptNames.h"

using Editor::ObjectKind;
using Editor::PlaceSettings;
using Editor::SceneEditor;
using SceneObjects::BodyType;

namespace
{
    const Vec3 BLUE = {0.30f, 0.50f, 0.90f};
    const Vec3 YELLOW = {0.95f, 0.85f, 0.30f};
    const Vec3 RED = {0.90f, 0.30f, 0.25f};
    const Vec3 GREY = {0.85f, 0.85f, 0.85f};
    const Vec3 GREEN = {0.35f, 0.75f, 0.35f};
    const Vec3 PURPLE = {0.60f, 0.40f, 0.85f};
    const Vec3 TEAL = {0.25f, 0.70f, 0.70f};

    PlaceSettings
    Brush(float w, float h, const Vec3& color, BodyType body, const std::string& tag = "")
    {
        PlaceSettings s;
        s.Width = w;
        s.Height = h;
        s.Color = color;
        s.Body = body;
        s.Tag = tag;
        return s;
    }

    // Point the scene's camera object at `target`, `distance` away
    void AimCamera(SceneEditor& editor, const Vec3& target, float distance)
    {
        SceneCamera::View view;
        view.Target = target;
        view.Distance = distance;
        editor.SetGameCamera(view);
    }

    Entity PlacePlayer(SceneEditor& editor, const Vec3& at)
    {
        Entity player = editor.Place(
                ObjectKind::Circle, at, Brush(1.0f, 1.0f, BLUE, BodyType::Dynamic, "Player"));
        editor.SetScript(player, ScriptNames::PlayerController);
        return player;
    }

    void PlacePickup(SceneEditor& editor, const Vec3& at)
    {
        PlaceSettings s = Brush(0.8f, 0.8f, YELLOW, BodyType::Trigger, "Pickup");
        s.Sides = 5;
        Entity pickup = editor.Place(ObjectKind::Polygon, at, s);
        editor.SetScript(pickup, ScriptNames::Collectible);
    }

    // Walls along the border of a W x H field
    void PlaceBorder(SceneEditor& editor, float w, float h)
    {
        PlaceSettings side = Brush(w, 0.6f, GREY, BodyType::Static, "Wall");
        side.Thickness = 0.8f;
        editor.Place(ObjectKind::Rectangle, {0, 0, h * 0.5f}, side);
        editor.Place(ObjectKind::Rectangle, {0, 0, -h * 0.5f}, side);
        side.YawDegrees = 90.0f;
        side.Width = h;
        editor.Place(ObjectKind::Rectangle, {w * 0.5f, 0, 0}, side);
        editor.Place(ObjectKind::Rectangle, {-w * 0.5f, 0, 0}, side);
    }

    // Just the field
    void Empty(SceneEditor& editor)
    {
        editor.NewScene();
    }

    // CollectGame level 1: pickups and walls
    void Level1(SceneEditor& editor)
    {
        editor.NewScene();
        editor.SetFieldSize(24.0f, 18.0f);
        PlaceBorder(editor, 24.0f, 18.0f);
        PlacePlayer(editor, {0, 0, -6});
        // An inner wall to walk around
        PlaceSettings wall = Brush(8.0f, 0.6f, GREY, BodyType::Static, "Wall");
        wall.Thickness = 0.8f;
        editor.Place(ObjectKind::Rectangle, {0, 0, 0}, wall);
        const Vec3 pickups[] = {
                {-8, 0, -5}, {8, 0, -5}, {-8, 0, 5}, {8, 0, 5}, {0, 0, 4}, {-3, 0, 6}};
        for (const Vec3& p : pickups)
            PlacePickup(editor, p);
        editor.SetSceneScript(ScriptNames::CollectGame);
        editor.SetSceneParam("Level", 1.0f);
        editor.SetSceneParam("Lives", 3.0f);
        AimCamera(editor, {0, 0, -1}, 24.0f);
    }

    // CollectGame level 2: patrolling hazards and a turret
    void Level2(SceneEditor& editor)
    {
        editor.NewScene();
        editor.SetFieldSize(28.0f, 20.0f);
        PlaceBorder(editor, 28.0f, 20.0f);
        PlacePlayer(editor, {0, 0, -8});
        // Two patrolling hazards crossing the field
        for (float z : {-2.0f, 3.0f})
        {
            PlaceSettings hazard = Brush(1.2f, 1.2f, RED, BodyType::Trigger, "Hazard");
            hazard.YawDegrees = 90.0f;
            Entity h = editor.Place(ObjectKind::Rectangle, {0, 0, z}, hazard);
            editor.SetScript(h, ScriptNames::MovingHazard);
            editor.SetScriptParam(h, "Distance", 9.0f);
            editor.SetScriptParam(h, "Speed", z < 0 ? 4.0f : 3.0f);
        }
        // A turret in the corner firing along the field
        PlaceSettings turret = Brush(1.4f, 1.4f, PURPLE, BodyType::Static, "Spawner");
        turret.YawDegrees = 90.0f;
        Entity t = editor.Place(ObjectKind::Triangle, {-12, 0, 7}, turret);
        editor.SetScript(t, ScriptNames::Spawner);
        editor.SetScriptParam(t, "Interval", 1.5f);
        const Vec3 pickups[] = {{-10, 0, -6},
                                {10, 0, -6},
                                {-10, 0, 7},
                                {10, 0, 7},
                                {0, 0, 8},
                                {5, 0, 0},
                                {-5, 0, 0}};
        for (const Vec3& p : pickups)
            PlacePickup(editor, p);
        editor.SetSceneScript(ScriptNames::CollectGame);
        editor.SetSceneParam("Level", 2.0f);
        editor.SetSceneParam("Lives", 3.0f);
        AimCamera(editor, {0, 0, -1}, 27.0f);
    }

    // A turret: an empty root, a static base, a spinning head and its barrel
    Prefab::Data TurretPrefab(SceneEditor& editor)
    {
        editor.NewScene(false);
        Entity root = editor.AddEmpty({0, 0, 0});
        editor.Rename(root, "Turret");
        Entity base = editor.Create(
                ObjectKind::Rectangle, {0, 0, 0}, root, Brush(1.2f, 1.2f, GREY, BodyType::Static));
        editor.Rename(base, "Base");
        PlaceSettings head = Brush(0.8f, 0.8f, RED, BodyType::None);
        head.Thickness = 0.5f;
        Entity h = editor.Create(ObjectKind::Circle, {0, 0, 0}, base, head);
        editor.Rename(h, "Head");
        editor.SetScript(h, ScriptNames::Rotator);
        editor.SetScriptParam(h, "Speed", 90.0f);
        PlaceSettings barrel = Brush(0.25f, 0.9f, GREY, BodyType::None);
        barrel.Thickness = 0.4f;
        Entity b = editor.Create(ObjectKind::Rectangle, {0, 0, 0.7f}, h, barrel);
        editor.Rename(b, "Barrel");
        return editor.CaptureStage("turret");
    }

    // Every shape type, crates, a chaser, 3D models, components and a hierarchy
    void Sandbox(SceneEditor& editor)
    {
        // The turret prefab is built first (in its own stage)
        Prefab::Data turret = TurretPrefab(editor);
        editor.NewScene();
        Entity player = PlacePlayer(editor, {0, 0, -8});
        // One of every shape, spinning
        const ObjectKind kinds[] = {ObjectKind::Rectangle,
                                    ObjectKind::Circle,
                                    ObjectKind::Triangle,
                                    ObjectKind::Polygon};
        const Vec3 colors[] = {RED, GREEN, PURPLE, TEAL};
        for (int i = 0; i < 4; ++i)
        {
            PlaceSettings s = Brush(2.0f, 1.2f, colors[i], BodyType::Static);
            s.Sides = 8;
            s.Thickness = 0.5f;
            Entity e = editor.Place(kinds[i], {-9.0f + 6.0f * i, 0, 6}, s);
            editor.SetScript(e, ScriptNames::Rotator);
            editor.SetScriptParam(e, "Speed", 30.0f + 30.0f * i);
        }
        // Crates to push around (dynamic bodies, no script)
        for (int i = 0; i < 3; ++i)
            editor.Place(ObjectKind::Rectangle,
                         {-3.0f + 3.0f * i, 0, -2},
                         Brush(1.0f, 1.0f, GREY, BodyType::Dynamic));
        // Something that chases the player
        Entity chaser = editor.Place(ObjectKind::Triangle,
                                     {10, 0, -10},
                                     Brush(1.0f, 1.2f, RED, BodyType::Trigger, "Enemy"));
        editor.SetScript(chaser, ScriptNames::Follower);
        editor.SetScriptParam(chaser, "Range", 12.0f);

        // Components (Scripts/Components): a walker going round a path of
        // Waypoint markers, and a zone that takes the player's Health
        Entity points[3];
        const Vec3 path[] = {{-12, 0, -5}, {-6, 0, -5}, {-9, 0, -10}};
        for (int i = 0; i < 3; ++i)
        {
            PlaceSettings marker = Brush(0.5f, 0.5f, YELLOW, BodyType::None);
            marker.Thickness = 0.05f;
            points[i] = editor.Place(ObjectKind::Circle, path[i], marker);
            editor.Rename(points[i], "Waypoint_" + std::to_string(i + 1));
            editor.AddComponent(points[i], ComponentNames::Waypoint);
        }
        for (int i = 0; i < 3; ++i)
            editor.SetField(points[i],
                            ComponentNames::Waypoint,
                            "Next",
                            static_cast<std::int64_t>(points[(i + 1) % 3]));
        editor.SetField(points[1], ComponentNames::Waypoint, "WaitSeconds", 1.0);
        Entity walker = editor.Place(
                ObjectKind::Polygon, {-12, 0, -8}, Brush(0.8f, 0.8f, BLUE, BodyType::None));
        editor.Rename(walker, "Walker");
        editor.SetScript(walker, ScriptNames::WaypointFollower);
        editor.AddComponent(walker, ComponentNames::Waypoint);
        editor.SetField(
                walker, ComponentNames::Waypoint, "Next", static_cast<std::int64_t>(points[0]));
        editor.AddComponent(walker, ComponentNames::Faction);
        editor.SetField(
                walker, ComponentNames::Faction, "Side", static_cast<std::int64_t>(Team::Neutral));
        editor.SetField(walker, ComponentNames::Faction, "Title", std::string("Patrol"));
        editor.SetField(walker, ComponentNames::Faction, "Banner", Reflection::FieldValue(BLUE));

        Entity zone = editor.Place(
                ObjectKind::Rectangle, {6, 0, -6}, Brush(2.0f, 2.0f, RED, BodyType::Trigger));
        editor.Rename(zone, "DamageZone");
        editor.SetThickness(zone, 0.05f);
        editor.SetScript(zone, ScriptNames::DamageZone);
        editor.AddComponent(player, ComponentNames::Health);
        editor.SetField(player, ComponentNames::Health, "DestroyAtZero", false);
        editor.AddComponent(player, ComponentNames::Faction);
        editor.SetField(
                player, ComponentNames::Faction, "Side", static_cast<std::int64_t>(Team::Player));
        editor.SetField(player, ComponentNames::Faction, "Title", std::string("Hero"));

        // Hierarchy: an empty spun by a Rotator carries its two children
        // around it
        Entity orbit = editor.AddEmpty({7, 0, 1});
        editor.Rename(orbit, "Orbit");
        editor.SetScript(orbit, ScriptNames::Rotator);
        editor.SetScriptParam(orbit, "Speed", 60.0f);
        for (int i = 0; i < 2; ++i)
        {
            Entity moon = editor.Place(ObjectKind::Circle,
                                       {5.0f + 4.0f * i, 0, 1},
                                       Brush(0.8f, 0.8f, YELLOW, BodyType::None));
            editor.Rename(moon, "Moon_" + std::to_string(i + 1));
            editor.SetParent(moon, orbit);
        }

        // Two instances of the turret prefab
        editor.PlacePrefab(turret, {-6, 0, 10});
        editor.PlacePrefab(turret, {6, 0, 10});

        // 3D models from data/models on the same field
        PlaceSettings model;
        model.Model = "Box";
        model.Width = 1.5f;
        Entity box = editor.Place(ObjectKind::Model, {12, 0, 0}, model);
        // Shaders: a glowing rim on the box, the ball bobs on a wave
        editor.SetFragmentShader(box, RimShaderID);
        model.Model = "GolfBall";
        model.Width = 1.0f;
        Entity ball = editor.Place(ObjectKind::Model, {-12, 0, 0}, model);
        editor.SetScript(ball, ScriptNames::Rotator);
        editor.SetVertexShader(ball, WaveVertShaderID);
        editor.SetHeight(ball, 1.0f);
        // A beacon: a tall, swaying, scanning column
        PlaceSettings beacon = Brush(0.8f, 0.8f, TEAL, BodyType::None);
        beacon.Thickness = 3.0f;
        beacon.Sides = 6;
        Entity column = editor.Place(ObjectKind::Polygon, {12, 0, -6}, beacon);
        editor.Rename(column, "Beacon");
        editor.SetFragmentShader(column, StripesShaderID);
        editor.SetVertexShader(column, SwayVertShaderID);
        AimCamera(editor, {0, 0, -1}, 30.0f);
    }

    // Metal Invasion: the field, the player's base and the game's scene
    // script. Everything else (crystals, units, enemies) is spawned by the
    // scripts while playing
    void MetalInvasionLevel(SceneEditor& editor)
    {
        editor.NewScene();
        editor.SetFieldSize(54.0f, 54.0f);
        editor.SetColor(editor.Objects()[0], {0.30f, 0.34f, 0.28f});

        PlaceSettings base;
        base.Model = MI::Models::Base;
        base.Width = MI::BASE_SCALE;
        base.Body = BodyType::Static;
        base.Tag = MI::Tags::Base;
        Entity b = editor.Place(ObjectKind::Model, {0, 0, 0}, base);
        editor.SetScript(b, MI::Scripts::Base);

        editor.SetSceneScript(MI::Scripts::Game);
        AimCamera(editor, {0, 0, -2}, 16.0f);
    }
} // namespace

namespace SampleScenes
{
    const std::vector<SampleScene>& All()
    {
        static const std::vector<SampleScene> scenes = {
                {"empty", Empty},
                {"level_1", Level1},
                {"level_2", Level2},
                {"sandbox", Sandbox},
                {"metal_invasion", MetalInvasionLevel, true},
        };
        return scenes;
    }

    const std::vector<SamplePrefab>& Prefabs()
    {
        static const std::vector<SamplePrefab> prefabs = {{"turret", TurretPrefab}};
        return prefabs;
    }

    Serialization::SaveFormat FormatOf(const SampleScene& scene)
    {
        return scene.PlainText ? Serialization::SaveFormat::Text
                               : Serialization::SaveFormat::Binary;
    }
} // namespace SampleScenes
