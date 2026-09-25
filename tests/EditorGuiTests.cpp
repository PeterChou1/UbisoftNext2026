//---------------------------------------------------------------------------------
// EditorGuiTests.cpp
//---------------------------------------------------------------------------------
//
// The scene editor GUI driven like a user would: mouse and keyboard through
// the stubbed App API, real GameManager frames. Widgets are found by the text
// they print, so the tests do not depend on exact pixel layouts
//
#include "AppStub.h"
#include "Camera.h"
#include "RenderConstants.h"
#include "SceneEditorScene.h"
#include "Scripting/ScriptRegistry.h"
#include "Scripting/ScriptSystem.h"
#include "Scripts/Components/GameComponents.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using SceneObjects::BodyType;

namespace
{
    constexpr float FRAME_MS = 16.0f;

    Editor::SceneEditor& Core() { return TestEnvironment::Editor().GetEditor(); }

    // Fresh editor session with a new scene, one frame rendered
    void OpenEditor()
    {
        AppStub::Reset();
        GameSceneManager.SetActiveScene(TestEnvironment::EDITOR_SCENE);
        // Park the mouse over the status bar (no hover effects)
        AppStub::Get().MouseX = 500.0f;
        AppStub::Get().MouseY = 20.0f;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    const AppStub::State::PrintedText* FindText(const std::string& text, float minX = -1.0f, bool prefix = false)
    {
        for (const auto& printed : AppStub::Get().Printed)
        {
            bool match = prefix ? printed.Text.compare(0, text.size(), text) == 0 : printed.Text == text;
            if (match && printed.X >= minX)
                return &printed;
        }
        return nullptr;
    }

    void Press(float x, float y)
    {
        AppStub::Get().MouseX = x;
        AppStub::Get().MouseY = y;
        AppStub::Get().LeftDown = true;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    void Release()
    {
        AppStub::Get().LeftDown = false;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    void Click(float x, float y)
    {
        Press(x, y);
        Release();
    }

    // Click the button showing `label` (centre of its text = centre of the button)
    bool ClickButton(const std::string& label, float minX = -1.0f)
    {
        const auto* text = FindText(label, minX);
        ++TestFramework::TotalChecks();
        if (text == nullptr)
        {
            TestFramework::ReportFailure(__FILE__, __LINE__, "no button '" + label + "'");
            return false;
        }
        Click(text->X + 5.0f * label.size(), text->Y + 5.0f);
        return true;
    }

    // Click the -/+ (or </>) button of the stepper row whose label starts with `prefix`
    bool ClickStepper(const std::string& prefix, int direction, float minX = -1.0f)
    {
        const auto* label = FindText(prefix, minX, true);
        ++TestFramework::TotalChecks();
        if (label == nullptr)
        {
            TestFramework::ReportFailure(__FILE__, __LINE__, "no stepper '" + prefix + "'");
            return false;
        }
        float rowY = label->Y;
        float rowX = label->X;
        for (const auto& printed : AppStub::Get().Printed)
        {
            bool isButton = direction > 0 ? (printed.Text == "+" || printed.Text == ">")
                                          : (printed.Text == "-" || printed.Text == "<");
            if (isButton && std::fabs(printed.Y - (rowY - 1.0f)) < 0.5f && printed.X > rowX &&
                printed.X < rowX + 240.0f)
            {
                Click(printed.X + 5.0f, printed.Y + 5.0f);
                return true;
            }
        }
        TestFramework::ReportFailure(__FILE__, __LINE__, "no button on stepper '" + prefix + "'");
        return false;
    }

    Vec2 ScreenOf(const Vec3& world) { return ECS.GetResource<Camera>()->WorldPointToScreenSpace(world); }

    void ClickGround(const Vec3& world)
    {
        Vec2 s = ScreenOf(world);
        Click(s.X, s.Y);
    }

    void PressKey(App::Key key)
    {
        AppStub::Get().Keys[key] = true;
        TestEnvironment::RunFrame(FRAME_MS);
        AppStub::Get().Keys[key] = false;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    constexpr float INSPECTOR_X = APP_VIRTUAL_WIDTH - 250.0f;

    Entity OnlyObject()
    {
        std::vector<Entity> objects = Core().Objects();
        return objects.size() == 2 ? objects[1] : NULL_ENTITY;
    }
} // namespace

TEST_CASE("Editor GUI: place shapes from the palette by clicking the field")
{
    OpenEditor();
    REQUIRE(Core().Objects().size() == 1);
    REQUIRE(ClickButton("3 Triangle"));
    CHECK(FindText("> 3 Triangle") != nullptr);
    ClickGround({2, 0, 3});
    REQUIRE(Core().Objects().size() == 2);
    Entity e = OnlyObject();
    CHECK(Core().KindOf(e) == Editor::ObjectKind::Triangle);
    // Snapped to the 0.5 grid
    Vec3 p = SceneObjects::GetPosition(e);
    CHECK(std::fabs(p.X - 2.0f) < 0.26f);
    CHECK(std::fabs(p.Z - 3.0f) < 0.26f);
    CHECK_EQ(std::fmod(p.X, 0.5f), 0.0f);
    CHECK(AppStub::WasPrinted("Placed Triangle"));

    // Keyboard shortcut for the next kind, right click stops placing
    PressKey(App::KEY_2);
    ClickGround({-3, 0, -2});
    CHECK_EQ(Core().Objects().size(), size_t(3));
    CHECK(Core().KindOf(Core().Objects()[2]) == Editor::ObjectKind::Circle);
    Vec2 s = ScreenOf({5, 0, 5});
    AppStub::Get().MouseX = s.X;
    AppStub::Get().MouseY = s.Y;
    AppStub::Get().RightDown = true;
    TestEnvironment::RunFrame(FRAME_MS);
    AppStub::Get().RightDown = false;
    TestEnvironment::RunFrame(FRAME_MS);
    ClickGround({5, 0, 5});
    CHECK_EQ(Core().Objects().size(), size_t(3));
}

TEST_CASE("Editor GUI: brush steppers set up what gets placed")
{
    OpenEditor();
    REQUIRE(ClickStepper("W ", +1));
    REQUIRE(ClickStepper("W ", +1));
    REQUIRE(ClickStepper("H ", -1));
    CHECK(FindText("W 1.50") != nullptr);
    // Body: None -> Static -> Dynamic
    REQUIRE(ClickStepper("None", +1));
    REQUIRE(ClickStepper("Static", +1));
    REQUIRE(ClickStepper("Tag ", +1, 0.0f));
    REQUIRE(ClickButton("1 Rectangle"));
    ClickGround({0, 0, 0});
    Entity e = OnlyObject();
    REQUIRE(e != NULL_ENTITY);
    const Shape2D& shape = ECS.GetComponent<Shape2D>(e);
    CHECK_EQ(shape.Width, 1.5f);
    CHECK_EQ(shape.Height, 0.75f);
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Dynamic);
    CHECK_EQ(ECS.GetComponent<SceneObject>(e).Tag, std::string("Player"));
}

TEST_CASE("Editor GUI: select and drag an object, one undo step")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0}, [] {
        Editor::PlaceSettings s;
        s.Width = 2.0f;
        s.Height = 2.0f;
        return s;
    }());
    Core().Select(NULL_ENTITY);
    size_t undo = Core().UndoCount();
    TestEnvironment::RunFrame(FRAME_MS);

    Vec2 start = ScreenOf({0, 0, 0});
    Press(start.X, start.Y);
    CHECK_EQ(Core().Selected(), e);
    // Drag over a few frames
    for (int i = 1; i <= 4; ++i)
    {
        Vec2 s = ScreenOf({i * 1.0f, 0, i * 0.5f});
        AppStub::Get().MouseX = s.X;
        AppStub::Get().MouseY = s.Y;
        TestEnvironment::RunFrame(FRAME_MS);
    }
    Release();
    Vec3 p = SceneObjects::GetPosition(e);
    CHECK(std::fabs(p.X - 4.0f) < 0.26f);
    CHECK(std::fabs(p.Z - 2.0f) < 0.26f);
    CHECK_EQ(Core().UndoCount(), undo + 1);
    CHECK(Core().Undo());
    CHECK_EQ(SceneObjects::GetPosition(e).X, 0.0f);

    // Clicking the field deselects without moving it
    Core().Select(e);
    ClickGround({8, 0, 8});
    CHECK(Core().IsField(Core().Selected()));
}

TEST_CASE("Editor GUI: the inspector edits the selected object")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Polygon, {0, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("Polygon  #", INSPECTOR_X, true) != nullptr);

    REQUIRE(ClickStepper("Size", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Width, 1.25f);
    REQUIRE(ClickStepper("Sides", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Sides, 7);
    REQUIRE(ClickStepper("Body ", -1, INSPECTOR_X));
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Trigger);
    REQUIRE(ClickStepper("Tag ", -1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<SceneObject>(e).Tag, std::string("Spawner"));

    // First object script, then its parameter
    REQUIRE(ClickStepper("Script ", +1, INSPECTOR_X));
    std::string first = ScriptRegistry::Get().Names(false).front();
    CHECK_EQ(Core().GetScript(e), first);
    const ScriptInfo* info = ScriptRegistry::Get().Find(first);
    REQUIRE(info != nullptr && !info->Params.empty());
    const ScriptParam& param = info->Params.front();
    REQUIRE(ClickStepper(param.Name.substr(0, 7), +1, INSPECTOR_X));
    CHECK_EQ(Core().GetScriptParam(e, param.Name), param.Default + param.Step);

    REQUIRE(ClickButton("Rot R", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetYaw(e), 15.0f);
    REQUIRE(ClickButton("Dup F", INSPECTOR_X));
    CHECK_EQ(Core().Objects().size(), size_t(3));
    Entity copy = Core().Selected();
    CHECK(copy != e);
    CHECK_EQ(Core().GetScript(copy), first);
    CHECK(ECS.GetResource<RenderConstants>()->EntityToVertexRange.count(copy) == 1);
    REQUIRE(ClickButton("Del X", INSPECTOR_X));
    CHECK_EQ(Core().Objects().size(), size_t(2));
    // Deleted during Render: its geometry still left the renderer
    CHECK(ECS.GetResource<RenderConstants>()->EntityToVertexRange.count(copy) == 0);

    // Toolbar undo brings the copy back
    REQUIRE(ClickButton("Undo"));
    CHECK_EQ(Core().Objects().size(), size_t(3));
    REQUIRE(ClickButton("Redo"));
    CHECK_EQ(Core().Objects().size(), size_t(2));
}

TEST_CASE("Editor GUI: keyboard shortcuts")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {1, 0, 1});
    PressKey(App::KEY_R);
    CHECK_EQ(SceneObjects::GetYaw(e), 15.0f);
    PressKey(App::KEY_F);
    CHECK_EQ(Core().Objects().size(), size_t(3));
    PressKey(App::KEY_X);
    CHECK_EQ(Core().Objects().size(), size_t(2));
    PressKey(App::KEY_U);
    CHECK_EQ(Core().Objects().size(), size_t(3));
    PressKey(App::KEY_Y);
    CHECK_EQ(Core().Objects().size(), size_t(2));

    // Holding W pans the camera forward
    Vec2 before = ScreenOf({0, 0, 0});
    AppStub::Get().Keys[App::KEY_W] = true;
    TestEnvironment::RunFrames(10, FRAME_MS);
    AppStub::Get().Keys[App::KEY_W] = false;
    TestEnvironment::RunFrame(FRAME_MS);
    Vec2 after = ScreenOf({0, 0, 0});
    CHECK(after.Y < before.Y);
}

TEST_CASE("Editor GUI: the scene tab sets the scene script and the field")
{
    OpenEditor();
    REQUIRE(ClickButton("Scene"));
    CHECK(FindText("SCENE", INSPECTOR_X) != nullptr);
    REQUIRE(ClickStepper("Script ", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->SceneScript, std::string("CollectGame"));
    REQUIRE(ClickStepper("Lives", +1, INSPECTOR_X));
    CHECK_EQ(Core().GetSceneParam("Lives"), 4.0f);
    float width = ECS.GetResource<SceneSettings>()->FieldWidth;
    REQUIRE(ClickStepper("Field W", -1, INSPECTOR_X));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->FieldWidth, width - 2.0f);
    CHECK_EQ(ECS.GetComponent<Shape2D>(Core().Objects()[0]).Width, width - 2.0f);
    REQUIRE(ClickButton("Game camera = view", INSPECTOR_X));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->CameraDistance, 30.0f);
    CHECK(FindText("Playable", INSPECTOR_X) != nullptr);
    REQUIRE(ClickButton("Object"));
    CHECK(FindText("OBJECT", INSPECTOR_X) != nullptr);
}

TEST_CASE("Editor GUI: Play runs the scripts, Stop restores the scene")
{
    OpenEditor();
    Entity spinner = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Core().SetScript(spinner, "Rotator");
    Entity ball = Core().Place(Editor::ObjectKind::Circle, {3, 0, 0}, [] {
        Editor::PlaceSettings s;
        s.Body = BodyType::Dynamic;
        return s;
    }());
    ECS.GetComponent<RigidBody>(ball).Velocity = Vec2(0, 2);
    Fixture::WorldImage authored = Fixture::Capture();

    REQUIRE(ClickButton("Play"));
    CHECK(Core().IsPlaying());
    CHECK(TestEnvironment::Editor().SimulatesWorld());
    TestEnvironment::RunFrames(20, FRAME_MS);
    CHECK(SceneObjects::GetYaw(spinner) > 10.0f);
    CHECK(SceneObjects::GetPosition(ball).Z > 0.2f);
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(1));
    CHECK(FindText("PLAYING", INSPECTOR_X) != nullptr);
    // Editing is disabled while playing
    CHECK(FindText("New") == nullptr);

    REQUIRE(ClickButton("Stop"));
    CHECK(!Core().IsPlaying());
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(0));
    CHECK_SAME_WORLD(authored, Fixture::Capture());

    // P toggles too
    PressKey(App::KEY_P);
    CHECK(Core().IsPlaying());
    PressKey(App::KEY_P);
    CHECK(!Core().IsPlaying());
}

TEST_CASE("Editor GUI: an unplayable scene does not start")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Circle, {0, 0, 0});
    ECS.AddComponent<ScriptComponent>(e, {"Ghost", {}});
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickButton("Play"));
    CHECK(!Core().IsPlaying());
    CHECK(AppStub::WasPrinted("Can not play"));
}

namespace
{
    // The editor working on its own scene folder (a temporary directory)
    struct SceneFolder
    {
        std::string Directory;
        explicit SceneFolder(const std::string& name)
        {
            Directory = (std::filesystem::temp_directory_path() / ("ubisoft_next_editor_" + name)).string();
            std::filesystem::remove_all(Directory);
            std::filesystem::create_directories(Directory);
            TestEnvironment::Editor().SetSceneDirectory(Directory);
            OpenEditor();
        }
        ~SceneFolder()
        {
            TestEnvironment::Editor().SetSceneDirectory(GameManager::SCENES_DIRECTORY);
            std::filesystem::remove_all(Directory);
        }
        bool Exists(const std::string& scene) const
        {
            return std::filesystem::exists(Directory + "/" + scene + ".ubsave");
        }
    };

    // Click a text field shown after `label` and type into it (Enter commits)
    bool TypeInto(const std::string& label, const std::string& text, float minX = -1.0f)
    {
        const auto* found = FindText(label, minX);
        ++TestFramework::TotalChecks();
        if (found == nullptr)
        {
            TestFramework::ReportFailure(__FILE__, __LINE__, "no field '" + label + "'");
            return false;
        }
        // Fields start 62 px after their label, rows are 22 px high
        Click(found->X + 62.0f + 20.0f, found->Y - 7.0f + 11.0f);
        AppStub::Type(text);
        TestEnvironment::RunFrame(FRAME_MS);
        return true;
    }

    SceneEditorScene& Gui() { return TestEnvironment::Editor(); }

    const AppStub::State::PrintedText* SceneList()
    {
        // The list is on the left of the toolbar (the scene name field is
        // further right)
        for (const auto& printed : AppStub::Get().Printed)
        {
            if (printed.X < 190.0f && printed.Y > APP_VIRTUAL_HEIGHT - 50.0f && printed.Text == Gui().SceneName())
                return &printed;
        }
        return nullptr;
    }
} // namespace

TEST_CASE("Editor GUI: type positions, rotation and sizes in the inspector")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);

    REQUIRE(TypeInto("Pos X", "3.5\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).X, 3.5f);
    REQUIRE(TypeInto("Pos Z", "-2.25\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).Z, -2.25f);
    REQUIRE(TypeInto("Rot", "45\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetYaw(e), 45.0f);
    REQUIRE(TypeInto("Width", "4\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Width, 4.0f);
    REQUIRE(TypeInto("Height", "0.5\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Height, 0.5f);
    REQUIRE(TypeInto("Thick", "1.5\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Thickness, 1.5f);
    // The values are shown in the fields
    CHECK(FindText("3.50", INSPECTOR_X) != nullptr);
    // Each typed value is one undo step
    CHECK(Core().Undo());
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Thickness, 0.25f);

    // Typed positions are kept on the field
    REQUIRE(TypeInto("Pos X", "9999\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).X, ECS.GetResource<SceneSettings>()->FieldWidth * 0.5f);

    // Backspace edits, Esc cancels, garbage is refused
    REQUIRE(TypeInto("Pos Z", "12\b\b7\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).Z, 7.0f);
    REQUIRE(TypeInto("Pos Z", "3\x1b", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).Z, 7.0f);
    REQUIRE(TypeInto("Pos Z", "-\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).Z, 7.0f);
    CHECK(AppStub::WasPrinted("Not a number"));

    // A model's scale
    Editor::PlaceSettings model;
    model.Model = "Box";
    Entity box = Core().Place(Editor::ObjectKind::Model, {-4, 0, -4}, model);
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(TypeInto("Scale", "2.5\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Transform>(box).LocalScale.X, 2.5f);
}

TEST_CASE("Editor GUI: typing goes to the field, not to the editor's shortcuts")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Circle, {1, 0, 1});
    TestEnvironment::RunFrame(FRAME_MS);
    const auto* label = FindText("Name", INSPECTOR_X);
    REQUIRE(label != nullptr);
    Click(label->X + 62.0f + 20.0f, label->Y - 7.0f + 11.0f);
    // x deletes, f duplicates, 1 picks the rectangle, wasd pans... none of
    // it may happen while typing (the keys are also reported as held)
    AppStub::Type("fox 1 wasd");
    AppStub::Get().Keys[App::KEY_X] = true;
    AppStub::Get().Keys[App::KEY_F] = true;
    TestEnvironment::RunFrame(FRAME_MS);
    AppStub::Get().Keys[App::KEY_X] = false;
    AppStub::Get().Keys[App::KEY_F] = false;
    AppStub::Type("\r");
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(Core().Objects().size(), size_t(2));
    CHECK_EQ(Core().NameOf(e), std::string("fox 1 wasd"));

    // Names must stay unique
    Entity other = Core().Place(Editor::ObjectKind::Rectangle, {4, 0, 4});
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(TypeInto("Name", "fox 1 wasd\r", INSPECTOR_X));
    CHECK(Core().NameOf(other) != std::string("fox 1 wasd"));
    CHECK(AppStub::WasPrinted("Can not rename"));
}

TEST_CASE("Editor GUI: drag a shape from the palette onto the field")
{
    OpenEditor();
    const auto* button = FindText("2 Circle");
    REQUIRE(button != nullptr);
    Press(button->X + 20.0f, button->Y + 5.0f);
    // Drag across the palette and the field, release on the field
    Vec2 target = ScreenOf({-4, 0, 3});
    for (int i = 1; i <= 5; ++i)
    {
        AppStub::Get().MouseX = button->X + (target.X - button->X) * i / 5.0f;
        AppStub::Get().MouseY = button->Y + (target.Y - button->Y) * i / 5.0f;
        TestEnvironment::RunFrame(FRAME_MS);
    }
    CHECK_EQ(Core().Objects().size(), size_t(1)); // nothing until released
    Release();
    REQUIRE(Core().Objects().size() == 2);
    Entity dropped = Core().Objects()[1];
    CHECK(Core().KindOf(dropped) == Editor::ObjectKind::Circle);
    CHECK(std::fabs(SceneObjects::GetPosition(dropped).X + 4.0f) < 0.3f);
    CHECK(std::fabs(SceneObjects::GetPosition(dropped).Z - 3.0f) < 0.3f);

    // Released back over the palette: nothing is placed
    button = FindText("> 2 Circle");
    REQUIRE(button != nullptr);
    Press(button->X + 20.0f, button->Y + 5.0f);
    Release();
    CHECK_EQ(Core().Objects().size(), size_t(2));
}

TEST_CASE("Editor GUI: place and drag in one gesture, pressing an object drags it")
{
    OpenEditor();
    REQUIRE(ClickButton("1 Rectangle"));
    // Press on the field places, keeping the button down drags the new object
    Vec2 start = ScreenOf({0, 0, 0});
    Press(start.X, start.Y);
    REQUIRE(Core().Objects().size() == 2);
    Entity placed = Core().Objects()[1];
    Vec2 end = ScreenOf({5, 0, -3});
    AppStub::Get().MouseX = end.X;
    AppStub::Get().MouseY = end.Y;
    TestEnvironment::RunFrame(FRAME_MS);
    Release();
    CHECK(std::fabs(SceneObjects::GetPosition(placed).X - 5.0f) < 0.3f);
    CHECK(std::fabs(SceneObjects::GetPosition(placed).Z + 3.0f) < 0.3f);
    // One undo removes it again
    CHECK(Core().Undo());
    CHECK_EQ(Core().Objects().size(), size_t(1));
    CHECK(Core().Redo());

    // Still placing: pressing the object grabs it instead of stacking a new one
    Vec2 on = ScreenOf({5, 0, -3});
    Press(on.X, on.Y);
    Vec2 moved = ScreenOf({-2, 0, 6});
    AppStub::Get().MouseX = moved.X;
    AppStub::Get().MouseY = moved.Y;
    TestEnvironment::RunFrame(FRAME_MS);
    Release();
    CHECK_EQ(Core().Objects().size(), size_t(2));
    CHECK(std::fabs(SceneObjects::GetPosition(placed).X + 2.0f) < 0.3f);
    CHECK(std::fabs(SceneObjects::GetPosition(placed).Z - 6.0f) < 0.3f);
}

TEST_CASE("Editor GUI: tall objects are picked where they are seen")
{
    OpenEditor();
    Editor::PlaceSettings tall;
    tall.Width = 1.0f;
    tall.Height = 1.0f;
    tall.Thickness = 4.0f;
    Entity tower = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0}, tall);
    Core().Select(NULL_ENTITY);
    TestEnvironment::RunFrame(FRAME_MS);
    // The top of the tower is drawn over the ground far behind it
    Vec2 top = ScreenOf({0, 4.0f, 0});
    Vec3 groundBehind;
    {
        Vec3 planePoint(0, 0, 0), normal(0, 1, 0);
        groundBehind = ECS.GetResource<Camera>()->ScreenSpaceToWorldPoint(top.X, top.Y, planePoint, normal);
    }
    CHECK(!SceneObjects::Contains(tower, groundBehind, 0.1f));
    Click(top.X, top.Y);
    CHECK_EQ(Core().Selected(), tower);
}

TEST_CASE("Editor GUI: New adds a saved scene to the list, rename it, switch scenes")
{
    SceneFolder folder("documents");
    // An empty folder starts with a new scene
    CHECK_EQ(Gui().SceneName(), std::string("scene_1"));
    CHECK(folder.Exists("scene_1"));

    REQUIRE(ClickButton("New"));
    CHECK_EQ(Gui().SceneName(), std::string("scene_2"));
    CHECK(folder.Exists("scene_2"));
    CHECK(Gui().SceneNames() == std::vector<std::string>({"scene_1", "scene_2"}));
    CHECK(SceneList() != nullptr);

    // Rename with the name field of the toolbar (spaces become '_')
    const auto* nameLabel = FindText("Name");
    REQUIRE(nameLabel != nullptr);
    Click(nameLabel->X + 60.0f, nameLabel->Y + 4.0f);
    AppStub::Type("my level\r");
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(Gui().SceneName(), std::string("my_level"));
    CHECK(folder.Exists("my_level"));
    CHECK(!folder.Exists("scene_2"));
    CHECK(Gui().SceneNames() == std::vector<std::string>({"my_level", "scene_1"}));
    // A name already used is refused
    CHECK(!Gui().RenameDocument("scene_1"));
    CHECK_EQ(Gui().SceneName(), std::string("my_level"));

    // Work on it and save
    Entity e = Core().Place(Editor::ObjectKind::Triangle, {2, 0, 2});
    Core().SetScript(e, "Rotator");
    Fixture::WorldImage saved = Fixture::Capture();
    REQUIRE(ClickButton("Save"));
    CHECK(!Core().IsDirty());

    // Pick scene_1 in the list: it opens
    const auto* list = SceneList();
    REQUIRE(list != nullptr);
    Click(list->X + 20.0f, list->Y + 5.0f);
    REQUIRE(ClickButton("scene_1"));
    CHECK_EQ(Gui().SceneName(), std::string("scene_1"));
    CHECK_EQ(Core().Objects().size(), size_t(1));

    // Unsaved changes: the first pick only warns, the second one discards
    Core().Place(Editor::ObjectKind::Circle, {0, 0, 0});
    list = SceneList();
    REQUIRE(list != nullptr);
    Click(list->X + 20.0f, list->Y + 5.0f);
    REQUIRE(ClickButton("my_level"));
    CHECK_EQ(Gui().SceneName(), std::string("scene_1"));
    CHECK(AppStub::WasPrinted("unsaved changes"));
    list = SceneList();
    REQUIRE(list != nullptr);
    Click(list->X + 20.0f, list->Y + 5.0f);
    REQUIRE(ClickButton("my_level"));
    CHECK_EQ(Gui().SceneName(), std::string("my_level"));
    saved.Settings.Name = "my_level";
    CHECK_SAME_WORLD(saved, Fixture::Capture());
    // Everything is rebuilt for the renderer after the world was replaced
    TestEnvironment::RunFrame(FRAME_MS);
    for (Entity shape : ECS.Visit<Shape2D>())
        CHECK(ECS.GetComponent<Shape2D>(shape).Built);

    // Revert throws away unsaved changes
    Core().Place(Editor::ObjectKind::Circle, {-3, 0, 0});
    REQUIRE(ClickButton("Revert"));
    CHECK_SAME_WORLD(saved, Fixture::Capture());

    // The saved scene plays in the Game's scene player
    std::string error;
    REQUIRE(GameSceneManager.LoadGame(Gui().ScenePath("my_level"), error));
    CHECK_EQ(GameSceneManager.GetActiveScene(), std::string(ScenePlayer::NAME));
    TestEnvironment::RunFrames(5, 20.0f);
    CHECK(GameSceneManager.Scripts().GetScript(e) != nullptr);
}

TEST_CASE("Editor GUI: add components, edit their generated fields, fold and remove them")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Entity other = Core().Place(Editor::ObjectKind::Circle, {4, 0, 0});
    Core().Select(e);
    TestEnvironment::RunFrame(FRAME_MS);

    REQUIRE(ClickButton("Components", INSPECTOR_X));
    CHECK(FindText("Transform", INSPECTOR_X) != nullptr);
    CHECK(FindText("Shape2D", INSPECTOR_X) != nullptr);
    CHECK(FindText("Pos X", INSPECTOR_X) == nullptr);

    // Pick Health in the Add picker (RigidBody, Script, Health, ...) and add it
    CHECK(FindText("Add RigidBody", INSPECTOR_X) != nullptr);
    REQUIRE(ClickStepper("Add ", 1, INSPECTOR_X));
    REQUIRE(ClickStepper("Add ", 1, INSPECTOR_X));
    REQUIRE(FindText("Add Health", INSPECTOR_X) != nullptr);
    REQUIRE(ClickButton("Add", INSPECTOR_X));
    REQUIRE(ECS.HasComponent<Health>(e));
    CHECK(FindText("- Health", INSPECTOR_X) != nullptr);

    // Widgets generated from the REFLECT block: number, check box
    REQUIRE(TypeInto("Current", "250\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Health>(e).Current, 250.0f);
    REQUIRE(TypeInto("Max", "0\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Health>(e).Max, 1.0f); // Range(1, 10000)
    const auto* invulnerable = FindText("Invuln.", INSPECTOR_X);
    REQUIRE(invulnerable != nullptr);
    Click(invulnerable->X + 70.0f, invulnerable->Y + 4.0f);
    CHECK(ECS.GetComponent<Health>(e).Invulnerable);

    // Faction: enum stepper, text, colour swatches, read only value
    REQUIRE(FindText("Add Faction", INSPECTOR_X) != nullptr);
    REQUIRE(ClickButton("Add", INSPECTOR_X));
    REQUIRE(ECS.HasComponent<Faction>(e));
    REQUIRE(ClickStepper("Side Neutral", 1, INSPECTOR_X));
    CHECK(ECS.GetComponent<Faction>(e).Side == Team::Player);
    CHECK(FindText("Side Player", INSPECTOR_X) != nullptr);
    REQUIRE(TypeInto("Title", "Blue team\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Faction>(e).Title, std::string("Blue team"));
    CHECK(FindText("Kills", INSPECTOR_X) != nullptr);
    const auto* banner = FindText("Banner", INSPECTOR_X);
    REQUIRE(banner != nullptr);
    Click(banner->X + 62.0f, banner->Y + 2.0f);
    CHECK(!(ECS.GetComponent<Faction>(e).Banner == Vec3(0.85f, 0.85f, 0.85f)));

    // Waypoint: type the name of the object to point at; tooltip on hover
    REQUIRE(FindText("Add Waypoint", INSPECTOR_X) != nullptr);
    REQUIRE(ClickButton("Add", INSPECTOR_X));
    REQUIRE(TypeInto("Next", Core().NameOf(other) + "\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).Next, other);
    REQUIRE(TypeInto("Next", "nobody\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).Next, other);
    CHECK(AppStub::WasPrinted("No object named nobody"));
    const auto* next = FindText("Next", INSPECTOR_X);
    REQUIRE(next != nullptr);
    AppStub::Get().MouseX = next->X + 5.0f;
    AppStub::Get().MouseY = next->Y;
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(AppStub::WasPrinted("Next: Object to go to next"));

    // Fold a component away, then remove Health (the first Remove)
    REQUIRE(ClickButton("- Health", INSPECTOR_X));
    CHECK(FindText("+ Health", INSPECTOR_X) != nullptr);
    CHECK(FindText("Current", INSPECTOR_X) == nullptr);
    REQUIRE(ClickButton("Remove", INSPECTOR_X));
    CHECK(!ECS.HasComponent<Health>(e));
    CHECK(ECS.HasComponent<Faction>(e));
    CHECK(AppStub::WasPrinted("Removed Health"));
    REQUIRE(Core().Undo());
    CHECK(ECS.HasComponent<Health>(e));

    // Back to the properties
    REQUIRE(ClickButton("Properties", INSPECTOR_X));
    CHECK(FindText("Pos X", INSPECTOR_X) != nullptr);
    Gui().ShowComponents(false);
}

TEST_CASE("Editor GUI: built in components are added and removed from the Components tab")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickButton("Components", INSPECTOR_X));
    REQUIRE(FindText("Add RigidBody", INSPECTOR_X) != nullptr);
    REQUIRE(ClickButton("Add", INSPECTOR_X));
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Static);
    CHECK(FindText("RigidBody", INSPECTOR_X) != nullptr);
    CHECK(FindText("Static", INSPECTOR_X) != nullptr);
    REQUIRE(ClickButton("Remove", INSPECTOR_X));
    CHECK(SceneObjects::GetBodyType(e) == BodyType::None);
    // The field has nothing to add
    Core().Select(Core().Objects()[0]);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("The field has no components", INSPECTOR_X) != nullptr);
    Gui().ShowComponents(false);
}

namespace
{
    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    // A row of the hierarchy tree (left panel) showing `name`
    const AppStub::State::PrintedText* TreeRow(const std::string& name)
    {
        for (const auto& printed : AppStub::Get().Printed)
        {
            if (printed.Text == name && printed.X < 180.0f && printed.Y > 50.0f)
                return &printed;
        }
        return nullptr;
    }

    // Press a tree row, drag it and release it over `to` (screen point)
    void DragRow(const AppStub::State::PrintedText& row, float toX, float toY)
    {
        float fromX = row.X + 10.0f;
        float fromY = row.Y + 4.0f;
        Press(fromX, fromY);
        AppStub::Get().MouseX = fromX + 3.0f;
        AppStub::Get().MouseY = (fromY + toY) * 0.5f;
        TestEnvironment::RunFrame(FRAME_MS);
        AppStub::Get().MouseX = toX;
        AppStub::Get().MouseY = toY;
        TestEnvironment::RunFrame(FRAME_MS);
        Release();
        // The tree shows the new parent from the next frame on
        TestEnvironment::RunFrame(FRAME_MS);
    }
} // namespace

TEST_CASE("Editor GUI: the hierarchy tree shows parents and children, drag rows to parent them")
{
    OpenEditor();
    Entity rect = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Entity circle = Core().Place(Editor::ObjectKind::Circle, {4, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickButton("Hierarchy"));
    CHECK(FindText("SCENE") != nullptr);
    const auto* field = TreeRow("Field");
    const auto* rectRow = TreeRow("Rectangle");
    const auto* circleRow = TreeRow("Circle");
    REQUIRE(field != nullptr);
    REQUIRE(rectRow != nullptr);
    REQUIRE(circleRow != nullptr);
    // Top level objects, the field first, one row each
    CHECK(field->Y > rectRow->Y);
    CHECK(rectRow->Y > circleRow->Y);
    CHECK_EQ(rectRow->X, circleRow->X);

    // Clicking a row selects the object
    Click(rectRow->X + 10.0f, rectRow->Y + 4.0f);
    CHECK_EQ(Core().Selected(), rect);

    // Drag Circle onto Rectangle: it becomes its child, indented under it
    rectRow = TreeRow("Rectangle");
    circleRow = TreeRow("Circle");
    DragRow(*circleRow, rectRow->X + 20.0f, rectRow->Y + 4.0f);
    CHECK_EQ(Core().ParentOf(circle), rect);
    CHECK(AppStub::WasPrinted("Circle is now a child of Rectangle"));
    CHECK(Near(SceneObjects::GetPosition(circle).X, 4.0f));
    rectRow = TreeRow("Rectangle");
    circleRow = TreeRow("Circle");
    REQUIRE(circleRow != nullptr);
    CHECK(circleRow->X > rectRow->X);

    // Rectangle can not go under its own child
    DragRow(*rectRow, circleRow->X + 10.0f, circleRow->Y + 4.0f);
    CHECK_EQ(Core().ParentOf(rect), NULL_ENTITY);
    CHECK(AppStub::WasPrinted("Can not put Rectangle under Circle"));

    // Fold Rectangle: its child's row disappears
    rectRow = TreeRow("Rectangle");
    const auto* fold = FindText("-", -1.0f);
    REQUIRE(fold != nullptr);
    CHECK(fold->X < rectRow->X);
    Click(fold->X + 2.0f, fold->Y + 4.0f);
    CHECK(TreeRow("Circle") == nullptr);
    REQUIRE(ClickButton("+"));
    REQUIRE(TreeRow("Circle") != nullptr);

    // Drop Circle on SCENE: top level again
    const auto* scene = FindText("SCENE");
    DragRow(*TreeRow("Circle"), scene->X + 10.0f, scene->Y + 4.0f);
    CHECK_EQ(Core().ParentOf(circle), NULL_ENTITY);
    CHECK(AppStub::WasPrinted("Circle is now a top level object"));

    // Selecting in the viewport shows the row selected in the tree; undo
    // brings the parent link back
    REQUIRE(Core().Undo());
    CHECK_EQ(Core().ParentOf(circle), rect);
    Gui().ShowHierarchy(false);
}

TEST_CASE("Editor GUI: New Empty, key 6, crosses and the Parent field")
{
    OpenEditor();
    Entity rect = Core().Place(Editor::ObjectKind::Rectangle, {-4, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickButton("Hierarchy"));
    // New Empty with Rectangle selected: an empty under it, at its position
    REQUIRE(ClickButton("New Empty"));
    Entity empty = Core().Selected();
    REQUIRE(SceneObjects::IsEmpty(empty));
    CHECK_EQ(Core().ParentOf(empty), rect);
    CHECK(Near(SceneObjects::GetPosition(empty).X, -4.0f));
    CHECK(TreeRow("Empty") != nullptr);

    // Empties are drawn as a cross in the viewport, at their position
    Core().Select(NULL_ENTITY);
    TestEnvironment::RunFrame(FRAME_MS);
    Vec2 at = ScreenOf(SceneObjects::GetPosition(empty));
    int crossLines = 0;
    for (const auto& line : AppStub::Get().Lines)
    {
        float mx = (line.X1 + line.X2) * 0.5f;
        float my = (line.Y1 + line.Y2) * 0.5f;
        if (std::fabs(mx - at.X) < 1.0f && std::fabs(my - at.Y) < 1.0f)
            ++crossLines;
    }
    CHECK(crossLines >= 2);

    // Key 6 places empties, clicking the cross selects it
    Gui().ShowHierarchy(false);
    PressKey(App::KEY_6);
    CHECK(FindText("> 6 Empty") != nullptr);
    ClickGround({5, 0, 3});
    Entity placed = OnlyObject() == NULL_ENTITY ? Core().Selected() : OnlyObject();
    REQUIRE(SceneObjects::IsEmpty(placed));
    CHECK(Near(SceneObjects::GetPosition(placed).X, 5.0f));
    PressKey(App::KEY_SPACE);
    Core().Select(NULL_ENTITY);
    ClickGround({5.1f, 0, 3});
    CHECK_EQ(Core().Selected(), placed);

    // The Properties tab: type the parent's name ("-" = top level)
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(TypeInto("Parent", "Rectangle\r", INSPECTOR_X));
    CHECK_EQ(Core().ParentOf(placed), rect);
    CHECK(Near(SceneObjects::GetPosition(placed).X, 5.0f));
    REQUIRE(TypeInto("Parent", "nobody\r", INSPECTOR_X));
    CHECK(AppStub::WasPrinted("No object named nobody"));
    REQUIRE(TypeInto("Parent", "-\r", INSPECTOR_X));
    CHECK_EQ(Core().ParentOf(placed), NULL_ENTITY);
    // Empties have no size / colour, only a scale
    CHECK(FindText("Scale", INSPECTOR_X) != nullptr);
    CHECK(FindText("Width", INSPECTOR_X) == nullptr);
}
