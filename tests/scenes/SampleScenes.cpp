#include "SampleScenes.h"

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

    PlaceSettings Brush(float w, float h, const Vec3& color, BodyType body, const std::string& tag = "")
    {
        PlaceSettings s;
        s.Width = w;
        s.Height = h;
        s.Color = color;
        s.Body = body;
        s.Tag = tag;
        return s;
    }

    Entity PlacePlayer(SceneEditor& editor, const Vec3& at)
    {
        Entity player = editor.Place(ObjectKind::Circle, at, Brush(1.0f, 1.0f, BLUE, BodyType::Dynamic, "Player"));
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

    void Empty(SceneEditor& editor)
    {
        editor.NewScene();
    }

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
        const Vec3 pickups[] = {{-8, 0, -5}, {8, 0, -5}, {-8, 0, 5}, {8, 0, 5}, {0, 0, 4}, {-3, 0, 6}};
        for (const Vec3& p : pickups)
            PlacePickup(editor, p);
        editor.SetSceneScript(ScriptNames::CollectGame);
        editor.SetSceneParam("Level", 1.0f);
        editor.SetSceneParam("Lives", 3.0f);
        editor.SetGameCamera({0, 0, -1}, 24.0f);
    }

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
        const Vec3 pickups[] = {{-10, 0, -6}, {10, 0, -6}, {-10, 0, 7}, {10, 0, 7}, {0, 0, 8}, {5, 0, 0}, {-5, 0, 0}};
        for (const Vec3& p : pickups)
            PlacePickup(editor, p);
        editor.SetSceneScript(ScriptNames::CollectGame);
        editor.SetSceneParam("Level", 2.0f);
        editor.SetSceneParam("Lives", 3.0f);
        editor.SetGameCamera({0, 0, -1}, 27.0f);
    }

    void Sandbox(SceneEditor& editor)
    {
        editor.NewScene();
        PlacePlayer(editor, {0, 0, -8});
        // One of every shape, spinning
        const ObjectKind kinds[] = {ObjectKind::Rectangle, ObjectKind::Circle, ObjectKind::Triangle, ObjectKind::Polygon};
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
            editor.Place(ObjectKind::Rectangle, {-3.0f + 3.0f * i, 0, -2}, Brush(1.0f, 1.0f, GREY, BodyType::Dynamic));
        // Something that chases the player
        Entity chaser = editor.Place(ObjectKind::Triangle, {10, 0, -10}, Brush(1.0f, 1.2f, RED, BodyType::Trigger, "Enemy"));
        editor.SetScript(chaser, ScriptNames::Follower);
        editor.SetScriptParam(chaser, "Range", 12.0f);
        // 3D models from data/models on the same field
        PlaceSettings model;
        model.Model = "Box";
        model.Width = 1.5f;
        editor.Place(ObjectKind::Model, {12, 0, 0}, model);
        model.Model = "GolfBall";
        model.Width = 1.0f;
        Entity ball = editor.Place(ObjectKind::Model, {-12, 0, 0}, model);
        editor.SetScript(ball, ScriptNames::Rotator);
        editor.SetGameCamera({0, 0, -1}, 30.0f);
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
        editor.SetGameCamera({0, 0, -2}, 16.0f);
    }
} // namespace

namespace SampleScenes
{
    const std::vector<SampleScene>& All()
    {
        static const std::vector<SampleScene> scenes = {
                {"empty", "Just the field", Empty},
                {"level_1", "CollectGame level 1: pickups and walls", Level1},
                {"level_2", "CollectGame level 2: patrolling hazards and a turret", Level2},
                {"sandbox", "Every shape type, crates, a chaser and 3D models", Sandbox},
                {"metal_invasion", "The original Metal Invasion game on scenes + scripts", MetalInvasionLevel, true},
        };
        return scenes;
    }
} // namespace SampleScenes

namespace SampleScenes
{
    Serialization::SaveFormat FormatOf(const SampleScene& scene)
    {
        return scene.PlainText ? Serialization::SaveFormat::Text : Serialization::SaveFormat::Binary;
    }
} // namespace SampleScenes
