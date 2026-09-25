//---------------------------------------------------------------------------------
// EditorTests.cpp
//---------------------------------------------------------------------------------
//
// Headless scene editor core (Editor::SceneEditor): placing and editing 2D
// shapes and models on the field, scripts, validation, files, undo / redo
// and play mode snapshots
//
#include "SceneEditor.h"
#include "Scripting/ScriptRegistry.h"
#include "WorldFixture.h"

#include <algorithm>
#include <filesystem>

using Editor::ObjectKind;
using Editor::PlaceSettings;
using Editor::SceneEditor;
using Fixture::Capture;
using SceneObjects::BodyType;

namespace
{
    std::string TempPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / ("ubisoft_next_editor_" + name)).string();
    }

    // Editor on a fresh world holding just the field
    SceneEditor& NewEditor()
    {
        static SceneEditor editor;
        Fixture::FreshWorld();
        editor = SceneEditor{};
        editor.NewScene();
        return editor;
    }

    PlaceSettings Brush(float w = 1.0f, float h = 1.0f, BodyType body = BodyType::None)
    {
        PlaceSettings s;
        s.Width = w;
        s.Height = h;
        s.Body = body;
        return s;
    }
} // namespace

TEST_CASE("Editor: a new scene is the field, the game camera and the light")
{
    SceneEditor& editor = NewEditor();
    std::vector<Entity> objects = editor.Objects();
    REQUIRE(objects.size() == 3);
    Entity field = objects[0];
    // The camera the scene plays with, where the old fixed camera was
    Entity camera = objects[1];
    CHECK_EQ(editor.GameCameraObject(), camera);
    CHECK(editor.IsCamera(camera));
    CHECK_EQ(editor.NameOf(camera), std::string("Main Camera"));
    CHECK_EQ(ECS.GetComponent<SceneObject>(camera).Tag, std::string("Camera"));
    CHECK(SceneObjects::IsEmpty(camera));
    SceneCamera::View view = SceneCamera::Current();
    CHECK_EQ(view.Distance, 30.0f);
    CHECK_EQ(view.Yaw, 0.0f);
    // The light, where the old fixed light was: above, shining at the centre
    Entity light = objects[2];
    CHECK_EQ(editor.LightObject(), light);
    CHECK(editor.IsLight(light));
    CHECK_EQ(editor.NameOf(light), std::string("Directional Light"));
    CHECK_EQ(ECS.GetComponent<SceneObject>(light).Tag, std::string("Light"));
    Vec3 aim = SceneLighting::GroundTarget(SceneLighting::Current());
    CHECK(std::fabs(aim.X) < 0.01f);
    CHECK(std::fabs(aim.Z) < 0.01f);
    CHECK(editor.IsField(field));
    CHECK_EQ(editor.NameOf(field), std::string("Field"));
    const Shape2D& shape = ECS.GetComponent<Shape2D>(field);
    auto settings = ECS.GetResource<SceneSettings>();
    CHECK_EQ(shape.Width, settings->FieldWidth);
    CHECK_EQ(shape.Height, settings->FieldHeight);
    CHECK(settings->SceneScript.empty());
    CHECK(!editor.IsDirty());
    CHECK_EQ(editor.UndoCount(), size_t(0));
    CHECK(editor.Validate().empty());
    // The field is protected
    CHECK(!editor.Remove(field));
    CHECK(!editor.Move(field, {3, 0, 3}));
    CHECK(editor.Duplicate(field) == NULL_ENTITY);
}

TEST_CASE("Editor: place every kind of object")
{
    SceneEditor& editor = NewEditor();
    for (int k = 0; k < static_cast<int>(ObjectKind::Count); ++k)
    {
        ObjectKind kind = static_cast<ObjectKind>(k);
        PlaceSettings s = Brush(1.5f, 0.75f, BodyType::Static);
        s.Sides = 7;
        s.YawDegrees = 45.0f;
        s.Color = Vec3(0.1f, 0.2f, 0.3f);
        s.Tag = "Wall";
        s.Model = "Box";
        Entity e = editor.Place(kind, {float(k) * 3.0f - 6.0f, 5.0f, 2.0f}, s);
        REQUIRE(e != NULL_ENTITY);
        CHECK(editor.KindOf(e) == kind);
        CHECK_EQ(editor.Selected(), e);
        CHECK_EQ(ECS.GetComponent<SceneObject>(e).Tag, std::string("Wall"));
        // Empties are only a transform: no body, whatever the brush says
        CHECK(SceneObjects::GetBodyType(e) == (kind == ObjectKind::Empty ? BodyType::None : BodyType::Static));
        CHECK_EQ(SceneObjects::GetYaw(e), 45.0f);
        // Objects stand on the field (y = 0)
        CHECK_EQ(SceneObjects::GetPosition(e).Y, 0.0f);
        CHECK_EQ(SceneObjects::GetPosition(e).X, float(k) * 3.0f - 6.0f);
        if (kind == ObjectKind::Model)
        {
            CHECK_EQ(ECS.GetComponent<Mesh>(e).Model, std::string("Box"));
            CHECK_EQ(ECS.GetComponent<Transform>(e).LocalScale.X, 1.5f);
            CHECK_EQ(editor.NameOf(e), std::string("Box"));
        }
        else if (kind == ObjectKind::Empty)
        {
            CHECK(SceneObjects::IsEmpty(e));
            CHECK(!ECS.HasComponent<Shape2D>(e));
            CHECK(!ECS.HasComponent<Mesh>(e));
            CHECK_EQ(editor.NameOf(e), std::string("Empty"));
        }
        else
        {
            const Shape2D& shape = ECS.GetComponent<Shape2D>(e);
            CHECK_EQ(shape.Width, 1.5f);
            CHECK_EQ(shape.Height, 0.75f);
            CHECK_EQ(shape.Sides, 7);
            CHECK(Fixture::Same(shape.Color, Vec3(0.1f, 0.2f, 0.3f)));
            CHECK_EQ(editor.NameOf(e), std::string(Editor::ObjectKindName(kind)));
        }
    }
    // + the field, the camera and the light
    CHECK_EQ(editor.Objects().size(), size_t(9));
    CHECK(editor.IsDirty());
    CHECK(editor.Validate().empty());

    // Models need a model name
    PlaceSettings noModel;
    CHECK(editor.Place(ObjectKind::Model, {0, 0, 0}, noModel) == NULL_ENTITY);
}

TEST_CASE("Editor: names are unique and positions stay on the field")
{
    SceneEditor& editor = NewEditor();
    Entity a = editor.Place(ObjectKind::Circle, {0, 0, 0});
    Entity b = editor.Place(ObjectKind::Circle, {1, 0, 0});
    Entity c = editor.Place(ObjectKind::Circle, {2, 0, 0});
    CHECK_EQ(editor.NameOf(a), std::string("Circle"));
    CHECK_EQ(editor.NameOf(b), std::string("Circle 2"));
    CHECK_EQ(editor.NameOf(c), std::string("Circle 3"));

    auto settings = ECS.GetResource<SceneSettings>();
    float halfW = settings->FieldWidth * 0.5f;
    Entity far = editor.Place(ObjectKind::Rectangle, {1000, 0, -1000});
    CHECK_EQ(SceneObjects::GetPosition(far).X, halfW);
    CHECK_EQ(SceneObjects::GetPosition(far).Z, -settings->FieldHeight * 0.5f);
    editor.Move(a, {-1000, 0, 3});
    CHECK_EQ(SceneObjects::GetPosition(a).X, -halfW);
    CHECK_EQ(SceneObjects::GetPosition(a).Z, 3.0f);

    // Shrinking the field brings objects back onto it
    editor.SetFieldSize(10.0f, 8.0f);
    CHECK_EQ(SceneObjects::GetPosition(a).X, -5.0f);
    CHECK_EQ(SceneObjects::GetPosition(far).Z, -4.0f);
    CHECK_EQ(ECS.GetComponent<Shape2D>(editor.Objects()[0]).Width, 10.0f);
    CHECK(editor.Validate().empty());
}

TEST_CASE("Editor: edit shape, size, colour, body, tag, model")
{
    SceneEditor& editor = NewEditor();
    Entity e = editor.Place(ObjectKind::Polygon, {0, 0, 0}, Brush(1.0f, 1.0f, BodyType::Dynamic));
    CHECK(editor.SetSize(e, 3.0f, 2.0f));
    CHECK(editor.SetSides(e, 9));
    CHECK(editor.SetThickness(e, 0.8f));
    CHECK(editor.SetColor(e, {1, 0, 1}));
    CHECK(editor.SetYaw(e, 400.0f));
    const Shape2D& shape = ECS.GetComponent<Shape2D>(e);
    CHECK_EQ(shape.Width, 3.0f);
    CHECK_EQ(shape.Sides, 9);
    CHECK_EQ(shape.Thickness, 0.8f);
    CHECK(!shape.Built); // rebuilt by the MeshHandler
    CHECK_EQ(SceneObjects::GetYaw(e), 40.0f);
    // The body follows the new shape
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Dynamic);
    CHECK(ECS.GetComponent<RigidBody>(e).Shape.PolygonPoints.size() == 9);

    CHECK(editor.SetBody(e, BodyType::Trigger));
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Trigger);
    CHECK(editor.SetTag(e, "Pickup"));
    CHECK(!editor.SetTag(e, "Field"));
    CHECK_EQ(ECS.GetComponent<SceneObject>(e).Tag, std::string("Pickup"));
    // Sides are clamped
    CHECK(editor.SetSides(e, 50));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Sides, 12);

    Entity model = editor.Place(ObjectKind::Model, {2, 0, 2}, [] {
        PlaceSettings s;
        s.Model = "Box";
        return s;
    }());
    CHECK(editor.SetModel(model, "GolfBall"));
    CHECK_EQ(ECS.GetComponent<Mesh>(model).Model, std::string("GolfBall"));
    CHECK(!ECS.GetComponent<Mesh>(model).Loaded);
    CHECK(!editor.SetModel(e, "Box")); // not a model object
    CHECK(editor.SetSize(model, 2.5f, 0.0f));
    CHECK_EQ(ECS.GetComponent<Transform>(model).LocalScale.X, 2.5f);

    // The field can be resized and recoloured through the same calls
    Entity field = editor.Objects()[0];
    CHECK(editor.SetColor(field, {0, 1, 0}));
    CHECK(editor.SetSize(field, 50.0f, 30.0f));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->FieldWidth, 50.0f);
}

TEST_CASE("Editor: pick prefers small objects, the field is last")
{
    SceneEditor& editor = NewEditor();
    Entity big = editor.Place(ObjectKind::Rectangle, {0, 0, 0}, Brush(6.0f, 6.0f));
    Entity small = editor.Place(ObjectKind::Circle, {1, 0, 1}, Brush(1.0f, 1.0f));
    CHECK_EQ(editor.Pick({1, 0, 1}), small);
    CHECK_EQ(editor.Pick({-2, 0, -2}), big);
    CHECK(editor.IsField(editor.Pick({10, 0, 10})));
    CHECK(editor.Pick({1000, 0, 1000}) == NULL_ENTITY);
}

TEST_CASE("Editor: duplicate copies shape, body, tag and script")
{
    SceneEditor& editor = NewEditor();
    PlaceSettings s = Brush(2.0f, 1.0f, BodyType::Trigger);
    s.Tag = "Hazard";
    Entity e = editor.Place(ObjectKind::Triangle, {0, 0, 0}, s);
    REQUIRE(editor.SetScript(e, "MovingHazard"));
    REQUIRE(editor.SetScriptParam(e, "Speed", 7.0f));
    Entity copy = editor.Duplicate(e);
    REQUIRE(copy != NULL_ENTITY);
    CHECK(copy != e);
    CHECK_EQ(editor.NameOf(copy), std::string("Triangle 2"));
    CHECK_EQ(editor.Selected(), copy);
    CHECK(Fixture::Same(ECS.GetComponent<Shape2D>(copy), ECS.GetComponent<Shape2D>(e)));
    CHECK(SceneObjects::GetBodyType(copy) == BodyType::Trigger);
    CHECK_EQ(ECS.GetComponent<SceneObject>(copy).Tag, std::string("Hazard"));
    CHECK_EQ(editor.GetScript(copy), std::string("MovingHazard"));
    CHECK_EQ(editor.GetScriptParam(copy, "Speed"), 7.0f);
    Vec3 offset = SceneObjects::GetPosition(copy) - SceneObjects::GetPosition(e);
    CHECK(offset.X != 0.0f || offset.Z != 0.0f);
}

TEST_CASE("Editor: object and scene scripts with parameters")
{
    SceneEditor& editor = NewEditor();
    Entity e = editor.Place(ObjectKind::Rectangle, {0, 0, 0});
    CHECK(editor.SetScript(e, "Rotator"));
    CHECK_EQ(editor.GetScript(e), std::string("Rotator"));
    // Declared default until changed
    CHECK_EQ(editor.GetScriptParam(e, "Speed"), 90.0f);
    CHECK(editor.SetScriptParam(e, "Speed", -30.0f));
    CHECK_EQ(editor.GetScriptParam(e, "Speed"), -30.0f);
    // Only declared parameters, registered object scripts
    CHECK(!editor.SetScriptParam(e, "Nope", 1.0f));
    CHECK(!editor.SetScript(e, "NoSuchScript"));
    CHECK(!editor.SetScript(e, "CollectGame"));
    CHECK_EQ(editor.GetScript(e), std::string("Rotator"));
    // Changing the script resets its parameters
    CHECK(editor.SetScript(e, "Patrol"));
    CHECK_EQ(editor.GetScriptParam(e, "Speed"), 2.0f);
    CHECK(editor.SetScript(e, ""));
    CHECK(!ECS.HasComponent<ScriptComponent>(e));

    CHECK(editor.SetSceneScript("CollectGame"));
    CHECK(!editor.SetSceneScript("Rotator"));
    CHECK_EQ(editor.GetSceneParam("Lives"), 3.0f);
    CHECK(editor.SetSceneParam("Lives", 5.0f));
    CHECK(!editor.SetSceneParam("Speed", 5.0f));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->SceneParams.at("Lives"), 5.0f);
    CHECK(editor.SetSceneScript(""));
    CHECK(ECS.GetResource<SceneSettings>()->SceneParams.empty());
}

TEST_CASE("Editor: validation finds what would break the game")
{
    SceneEditor& editor = NewEditor();
    Entity a = editor.Place(ObjectKind::Circle, {0, 0, 0});
    Entity b = editor.Place(ObjectKind::Circle, {1, 0, 0});
    CHECK(editor.Validate().empty());

    ECS.GetComponent<SceneObject>(b).Name = editor.NameOf(a);
    ECS.AddComponent<ScriptComponent>(a, {"Ghost", {}});
    SceneObjects::SetPosition(b, {500, 0, 0});
    ECS.GetResource<SceneSettings>()->SceneScript = "Rotator";
    std::vector<std::string> issues = editor.Validate();
    CHECK_EQ(issues.size(), size_t(4));
    auto mentions = [&](const std::string& text) {
        return std::any_of(issues.begin(), issues.end(),
                           [&](const std::string& i) { return i.find(text) != std::string::npos; });
    };
    CHECK(mentions("Duplicate"));
    CHECK(mentions("outside the field"));
    CHECK(mentions("Ghost"));
    CHECK(mentions("scene script"));

    // Invalid scenes are not saved
    Serialization::SaveResult result = editor.SaveScene(TempPath("invalid.ubsave"), "invalid");
    CHECK(!result.Success);
    CHECK(result.Error.find("not playable") != std::string::npos);
    CHECK(!std::filesystem::exists(TempPath("invalid.ubsave")));
}

TEST_CASE("Editor: save and load a scene file")
{
    SceneEditor& editor = NewEditor();
    Entity e = editor.Place(ObjectKind::Polygon, {3, 0, -2}, Brush(2.0f, 1.0f, BodyType::Static));
    editor.SetScript(e, "Rotator");
    editor.SetSceneScript("CollectGame");
    SceneCamera::View view;
    view.Target = {1, 0, 1};
    view.Distance = 25.0f;
    view.Yaw = 30.0f;
    editor.SetGameCamera(view);
    Fixture::WorldImage saved = Capture();

    std::string path = TempPath("scene.ubsave");
    Serialization::SaveResult result = editor.SaveScene(path, "my level");
    REQUIRE(result.Success);
    CHECK(!editor.IsDirty());
    CHECK_EQ(ECS.GetResource<SceneSettings>()->Name, std::string("my level"));

    int replaced = 0;
    editor.OnWorldReplaced = [&] { ++replaced; };
    editor.NewScene();
    // The field, the Main Camera and the light
    CHECK_EQ(editor.Objects().size(), size_t(3));

    Serialization::LoadResult loaded = editor.LoadScene(path);
    REQUIRE(loaded.Success);
    CHECK(loaded.Warnings.empty());
    CHECK_EQ(loaded.Metadata["Scene"], std::string(ScenePlayer::NAME));
    CHECK_EQ(loaded.Metadata["Name"], std::string("my level"));
    CHECK_EQ(loaded.Metadata["Tool"], std::string("SceneEditor 2"));
    CHECK_EQ(replaced, 2);
    CHECK(!editor.IsDirty());
    CHECK_EQ(editor.UndoCount(), size_t(0));
    saved.Settings.Name = "my level";
    CHECK_SAME_WORLD(saved, Capture());
    editor.OnWorldReplaced = nullptr;
    std::filesystem::remove(path);
}

TEST_CASE("Editor: loading refuses files that are not scenes and keeps the scene")
{
    SceneEditor& editor = NewEditor();
    editor.Place(ObjectKind::Circle, {0, 0, 0});
    // A save made by another scene (not an authored scene)
    std::string path = TempPath("not_a_scene.ubsave");
    REQUIRE(Fixture::Serializer().SaveToFile(ECS, path, {{"Scene", "Menu"}}).Success);
    Fixture::WorldImage before = Capture();
    Serialization::LoadResult result = editor.LoadScene(path);
    CHECK(!result.Success);
    CHECK(result.Error.find("not an authored scene") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture());

    result = editor.LoadScene(TempPath("missing.ubsave"));
    CHECK(!result.Success);
    CHECK_SAME_WORLD(before, Capture());
    std::filesystem::remove(path);
}

TEST_CASE("Editor: undo / redo every kind of edit")
{
    SceneEditor& editor = NewEditor();
    std::vector<Fixture::WorldImage> states{Capture()};
    Entity e = editor.Place(ObjectKind::Rectangle, {0, 0, 0});
    states.push_back(Capture());
    editor.Move(e, {2, 0, 2});
    states.push_back(Capture());
    editor.SetColor(e, {1, 0, 0});
    states.push_back(Capture());
    editor.SetBody(e, BodyType::Dynamic);
    states.push_back(Capture());
    editor.SetScript(e, "Rotator");
    states.push_back(Capture());
    editor.SetSceneScript("CollectGame");
    states.push_back(Capture());
    editor.Remove(e);
    states.push_back(Capture());
    CHECK_EQ(editor.UndoCount(), states.size() - 1);

    for (size_t i = states.size() - 1; i > 0; --i)
    {
        REQUIRE(editor.Undo());
        CHECK_SAME_WORLD(states[i - 1], Capture());
    }
    CHECK(!editor.Undo());
    for (size_t i = 1; i < states.size(); ++i)
    {
        REQUIRE(editor.Redo());
        CHECK_SAME_WORLD(states[i], Capture());
    }
    CHECK(!editor.Redo());

    // A new edit drops the redo history
    editor.Undo();
    editor.Place(ObjectKind::Circle, {1, 0, 1});
    CHECK_EQ(editor.RedoCount(), size_t(0));
}

TEST_CASE("Editor: the undo history is bounded")
{
    SceneEditor& editor = NewEditor();
    for (size_t i = 0; i < SceneEditor::MAX_UNDO + 10; ++i)
        editor.Place(ObjectKind::Circle, {float(i % 10), 0, float(i / 10)});
    CHECK_EQ(editor.UndoCount(), SceneEditor::MAX_UNDO);
}

TEST_CASE("Editor: play mode restores the authored scene exactly")
{
    SceneEditor& editor = NewEditor();
    Entity e = editor.Place(ObjectKind::Circle, {0, 0, 0}, Brush(1, 1, BodyType::Dynamic));
    editor.SetScript(e, "Rotator");
    Fixture::WorldImage authored = Capture();

    editor.BeginPlay();
    CHECK(editor.IsPlaying());
    // Whatever the game does while playing...
    SceneObjects::SetPosition(e, {5, 0, 5});
    SceneObjects::Destroy(e);
    SceneObjects::CreateShape(Fixture::ShapeOf("Spawned", Shape2DType::Circle, {1, 0, 1}));
    ECS.GetResource<SceneSettings>()->CameraDistance = 1.0f;
    ECS.FlushECS();
    // ...and editing is blocked
    CHECK(!editor.Undo());

    editor.EndPlay();
    CHECK(!editor.IsPlaying());
    CHECK_SAME_WORLD(authored, Capture());
}
