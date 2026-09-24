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

    REQUIRE(ClickStepper("Size ", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Width, 1.25f);
    REQUIRE(ClickStepper("Sides ", +1, INSPECTOR_X));
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
    REQUIRE(ClickStepper(param.Name + " ", +1, INSPECTOR_X));
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
    REQUIRE(ClickStepper("Lives ", +1, INSPECTOR_X));
    CHECK_EQ(Core().GetSceneParam("Lives"), 4.0f);
    float width = ECS.GetResource<SceneSettings>()->FieldWidth;
    REQUIRE(ClickStepper("Field W ", -1, INSPECTOR_X));
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

TEST_CASE("Editor GUI: choose a slot in the scene list, save, change, load")
{
    const std::string slot = "my_scene_3";
    const std::string path = GameManager::ScenePath(slot);
    const std::string backup = path + ".testbackup";
    bool existed = std::filesystem::exists(path);
    if (existed)
        std::filesystem::rename(path, backup);

    OpenEditor();
    // The list starts on the first scene, open it and pick the slot
    // (the toolbar also prints the scene name further right)
    const AppStub::State::PrintedText* list = nullptr;
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.Text == "empty" && printed.X < 190.0f)
            list = &printed;
    }
    REQUIRE(list != nullptr);
    Click(list->X + 25.0f, list->Y + 5.0f);
    REQUIRE(ClickButton(slot));
    CHECK(FindText(slot) != nullptr);
    // The click on the list entry did not reach the widgets under it
    CHECK_EQ(Core().Objects().size(), size_t(1));

    Entity e = Core().Place(Editor::ObjectKind::Triangle, {2, 0, 2});
    Core().SetScript(e, "Rotator");
    Fixture::WorldImage saved = Fixture::Capture();
    REQUIRE(ClickButton("Save"));
    CHECK(std::filesystem::exists(path));
    CHECK(AppStub::WasPrinted("Saved"));

    REQUIRE(ClickButton("New"));
    CHECK_EQ(Core().Objects().size(), size_t(1));
    REQUIRE(ClickButton("Load"));
    CHECK_EQ(Core().Objects().size(), size_t(2));
    saved.Settings.Name = slot;
    CHECK_SAME_WORLD(saved, Fixture::Capture());
    // Everything is rebuilt for the renderer after the world was replaced
    for (Entity shape : ECS.Visit<Shape2D>())
        CHECK(ECS.GetComponent<Shape2D>(shape).Built);

    // The saved scene plays in the Game's scene player
    std::string error;
    REQUIRE(GameSceneManager.LoadGame(path, error));
    CHECK_EQ(GameSceneManager.GetActiveScene(), std::string(ScenePlayer::NAME));
    TestEnvironment::RunFrames(5, 20.0f);
    CHECK(GameSceneManager.Scripts().GetScript(e) != nullptr);

    std::filesystem::remove(path);
    if (existed)
        std::filesystem::rename(backup, path);
}
