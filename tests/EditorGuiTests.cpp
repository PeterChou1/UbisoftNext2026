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
#include "EditorStyle.h"
#include "Lighting.h"
#include "RenderConstants.h"
#include "SceneEditorScene.h"
#include "Scripting/ScriptRegistry.h"
#include "Scripting/ScriptSystem.h"
#include "Scripts/Components/GameComponents.h"
#include "UIText.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using SceneObjects::BodyType;

namespace
{
    constexpr float FRAME_MS = 16.0f;
    // Objects of a new scene: the field, the Main Camera and the light
    constexpr std::size_t BASE = 3;
    constexpr float INSPECTOR_X = APP_VIRTUAL_WIDTH - EditorStyle::INSPECTOR_W;
    constexpr float LEFT_W = EditorStyle::LEFT_W;

    Editor::SceneEditor& Core() { return TestEnvironment::Editor().GetEditor(); }
    SceneEditorScene& Gui() { return TestEnvironment::Editor(); }

    // Fresh editor session with a new scene, one frame rendered
    void OpenEditor()
    {
        AppStub::Reset();
        if (Gui().InPrefabMode())
        {
            Gui().BackToScene();
            Gui().BackToScene();
        }
        Gui().Menu().Close();
        Gui().StartPlacingModel("");
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

    void RightClick(float x, float y)
    {
        AppStub::Get().MouseX = x;
        AppStub::Get().MouseY = y;
        AppStub::Get().RightDown = true;
        TestEnvironment::RunFrame(FRAME_MS);
        AppStub::Get().RightDown = false;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    void MoveMouse(float x, float y)
    {
        AppStub::Get().MouseX = x;
        AppStub::Get().MouseY = y;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    // Click the button showing `label` (the middle of the label is the
    // middle of the button)
    bool ClickButton(const std::string& label, float minX = -1.0f)
    {
        const auto* text = FindText(label, minX);
        ++TestFramework::TotalChecks();
        if (text == nullptr)
        {
            TestFramework::ReportFailure(__FILE__, __LINE__, "no button '" + label + "'");
            return false;
        }
        Click(text->X + UIText::Width(label) * 0.5f, text->Y + UIText::CapHeight() * 0.5f);
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
            // Labels and button texts are centered on the same row
            if (isButton && std::fabs(printed.Y - rowY) < 1.5f && printed.X > rowX && printed.X < rowX + 240.0f)
            {
                Click(printed.X + 3.0f, printed.Y + UIText::CapHeight() * 0.5f);
                return true;
            }
        }
        TestFramework::ReportFailure(__FILE__, __LINE__, "no button on stepper '" + prefix + "'");
        return false;
    }

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
        // Fields start 70 units after their label, in the middle of the row
        Click(found->X + 70.0f + 20.0f, found->Y + UIText::CapHeight() * 0.5f);
        AppStub::Type(text);
        TestEnvironment::RunFrame(FRAME_MS);
        return true;
    }

    // Click an item of the open context menu (hovering `path` first to open
    // the submenus)
    bool ClickMenu(const std::vector<std::string>& path)
    {
        ++TestFramework::TotalChecks();
        for (std::size_t i = 0; i < path.size(); ++i)
        {
            // The menu is drawn last: its item is the last text with that label
            const AppStub::State::PrintedText* item = nullptr;
            for (const auto& printed : AppStub::Get().Printed)
            {
                if (printed.Text == path[i])
                    item = &printed;
            }
            if (item == nullptr)
            {
                TestFramework::ReportFailure(__FILE__, __LINE__, "no menu item '" + path[i] + "'");
                return false;
            }
            float x = item->X + 4.0f;
            float y = item->Y + UIText::CapHeight() * 0.5f;
            if (i + 1 < path.size())
                MoveMouse(x, y);
            else
                Click(x, y);
        }
        return true;
    }

    Vec2 ScreenOf(const Vec3& world) { return ECS.GetResource<Camera>()->WorldPointToScreenSpace(world); }

    void ClickGround(const Vec3& world)
    {
        Vec2 s = ScreenOf(world);
        Click(s.X, s.Y);
    }

    void RightClickGround(const Vec3& world)
    {
        Vec2 s = ScreenOf(world);
        RightClick(s.X, s.Y);
    }

    void PressKey(App::Key key)
    {
        AppStub::Get().Keys[key] = true;
        TestEnvironment::RunFrame(FRAME_MS);
        AppStub::Get().Keys[key] = false;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    Entity OnlyObject()
    {
        // After the field, the Main Camera and the light of a new scene
        std::vector<Entity> objects = Core().Objects();
        return objects.size() == BASE + 1 ? objects[BASE] : NULL_ENTITY;
    }

    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    // A row of the hierarchy tree (left panel, above the assets) showing `name`
    const AppStub::State::PrintedText* TreeRow(const std::string& name)
    {
        for (const auto& printed : AppStub::Get().Printed)
        {
            if (printed.Text == name && printed.X < LEFT_W && printed.Y > 300.0f && printed.Y < 700.0f)
                return &printed;
        }
        return nullptr;
    }

    // A row of the assets list (left panel, bottom) showing `name`
    const AppStub::State::PrintedText* AssetRow(const std::string& name)
    {
        for (const auto& printed : AppStub::Get().Printed)
        {
            if (printed.Text == name && printed.X < LEFT_W && printed.Y > 50.0f && printed.Y < 290.0f)
                return &printed;
        }
        return nullptr;
    }

    // Press a row, drag it and release it over `to` (screen point)
    void DragRow(const AppStub::State::PrintedText& row, float toX, float toY)
    {
        float fromX = row.X + 10.0f;
        float fromY = row.Y + 4.0f;
        Press(fromX, fromY);
        MoveMouse(fromX + 3.0f, (fromY + toY) * 0.5f);
        MoveMouse(toX, toY);
        Release();
        // The tree shows the new parent from the next frame on
        TestEnvironment::RunFrame(FRAME_MS);
    }

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

    // The editor working on its own prefab folder
    struct PrefabFolder
    {
        std::string Directory;
        PrefabFolder()
        {
            Directory = (std::filesystem::temp_directory_path() / "ubisoft_next_editor_prefabs").string();
            std::filesystem::remove_all(Directory);
            std::filesystem::create_directories(Directory);
            Gui().SetPrefabDirectory(Directory);
        }
        ~PrefabFolder()
        {
            Gui().SetPrefabDirectory(Prefab::DIRECTORY);
            std::filesystem::remove_all(Directory);
        }
        bool Exists(const std::string& prefab) const { return std::filesystem::exists(Prefab::PathOf(prefab, Directory)); }
    };

    const AppStub::State::PrintedText* SceneList()
    {
        // The list is on the left of the toolbar (the scene name field is
        // further right)
        for (const auto& printed : AppStub::Get().Printed)
        {
            if (printed.X < LEFT_W && printed.Y > APP_VIRTUAL_HEIGHT - 50.0f && printed.Text == Gui().SceneName())
                return &printed;
        }
        return nullptr;
    }
} // namespace

//-----------------------------------------------------------------------------
// Creating objects: context menus
//-----------------------------------------------------------------------------

TEST_CASE("Editor GUI: right click the ground to create objects there")
{
    OpenEditor();
    REQUIRE(Core().Objects().size() == BASE);
    RightClickGround({2, 0, 3});
    REQUIRE(Gui().Menu().IsOpen());
    CHECK(FindText("Create Empty") != nullptr);
    CHECK(FindText("Create Model") != nullptr);
    REQUIRE(ClickMenu({"Create Triangle"}));
    CHECK(!Gui().Menu().IsOpen());
    REQUIRE(Core().Objects().size() == BASE + 1);
    Entity e = OnlyObject();
    CHECK(Core().KindOf(e) == Editor::ObjectKind::Triangle);
    CHECK_EQ(Core().Selected(), e);
    // Snapped to the 0.5 grid where the menu was opened
    Vec3 p = SceneObjects::GetPosition(e);
    CHECK(std::fabs(p.X - 2.0f) < 0.26f);
    CHECK(std::fabs(p.Z - 3.0f) < 0.26f);
    CHECK_EQ(std::fmod(p.X, 0.5f), 0.0f);
    CHECK(AppStub::WasPrinted("Created Triangle"));

    // Submenus: Create Model > Box
    RightClickGround({-4, 0, -2});
    REQUIRE(ClickMenu({"Create Model", "Box"}));
    REQUIRE(Core().Objects().size() == BASE + 2);
    CHECK(Core().KindOf(Core().Objects()[BASE + 1]) == Editor::ObjectKind::Model);

    // A click elsewhere, a right click or Esc closes the menu without creating
    RightClickGround({5, 0, 5});
    REQUIRE(Gui().Menu().IsOpen());
    ClickGround({6, 0, 6});
    CHECK(!Gui().Menu().IsOpen());
    RightClickGround({5, 0, 5});
    AppStub::Type("\x1b");
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(!Gui().Menu().IsOpen());
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    // Each creation is one undo step
    CHECK(Core().Undo());
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));
}

TEST_CASE("Editor GUI: an object's context menu creates children, renames, duplicates and deletes")
{
    OpenEditor();
    Entity parent = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Core().Select(NULL_ENTITY);
    TestEnvironment::RunFrame(FRAME_MS);

    // Right click on the object: it is selected and its menu opens
    RightClickGround({0, 0, 0});
    CHECK_EQ(Core().Selected(), parent);
    REQUIRE(ClickMenu({"Create Child", "Create Circle"}));
    REQUIRE(Core().ChildrenOf(parent).size() == 1u);
    Entity child = Core().ChildrenOf(parent)[0];
    CHECK(Core().KindOf(child) == Editor::ObjectKind::Circle);
    CHECK(AppStub::WasPrinted("Created Circle in Rectangle"));
    // Next to its parent
    CHECK(Near(SceneObjects::GetPosition(child).X, 1.0f, 0.3f));

    // Unparent from the child's menu (in the hierarchy tree)
    TestEnvironment::RunFrame(FRAME_MS);
    const auto* row = FindText("Circle", -1.0f);
    REQUIRE(row != nullptr);
    RightClick(row->X + 4.0f, row->Y + 4.0f);
    REQUIRE(ClickMenu({"Unparent"}));
    CHECK_EQ(Core().ParentOf(child), NULL_ENTITY);

    // Rename: type straight away
    RightClickGround({0, 0, 0});
    REQUIRE(ClickMenu({"Rename"}));
    AppStub::Type("Base\r");
    TestEnvironment::RunFrame(FRAME_MS);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(Core().NameOf(parent), std::string("Base"));

    // Duplicate and Delete
    RightClickGround({0, 0, 0});
    REQUIRE(ClickMenu({"Duplicate"}));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 3));
    Entity copy = Core().Selected();
    CHECK(copy != parent);
    Vec2 s = ScreenOf(SceneObjects::GetPosition(copy));
    RightClick(s.X, s.Y);
    REQUIRE(ClickMenu({"Delete"}));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    CHECK(AppStub::WasPrinted("Deleted"));
}

TEST_CASE("Editor GUI: the hierarchy's + button and a right click on its empty part create objects")
{
    OpenEditor();
    TestEnvironment::RunFrame(FRAME_MS);
    const auto* title = FindText("HIERARCHY");
    REQUIRE(title != nullptr);
    // "+" is on the title row, at the right of the panel
    const AppStub::State::PrintedText* plus = nullptr;
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.Text == "+" && printed.X < LEFT_W && std::fabs(printed.Y - title->Y) < 1.5f)
            plus = &printed;
    }
    REQUIRE(plus != nullptr);
    Click(plus->X + 2.0f, plus->Y + 4.0f);
    REQUIRE(Gui().Menu().IsOpen());
    REQUIRE(ClickMenu({"Create Empty"}));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));
    CHECK(SceneObjects::IsEmpty(OnlyObject()));

    // Empty part of the tree
    RightClick(60.0f, 400.0f);
    REQUIRE(ClickMenu({"Create Polygon"}));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    CHECK(Core().KindOf(Core().Selected()) == Editor::ObjectKind::Polygon);
}

//-----------------------------------------------------------------------------
// Assets
//-----------------------------------------------------------------------------

TEST_CASE("Editor GUI: place models and prefabs from the Assets list")
{
    OpenEditor();
    const auto* box = AssetRow("Box");
    REQUIRE(box != nullptr);
    CHECK(AssetRow("turret") != nullptr);
    Click(box->X + 4.0f, box->Y + 4.0f);
    CHECK_EQ(Gui().PlacingAsset(), std::string("Box"));
    // Every click on the scene places one, until a right click
    ClickGround({3, 0, 3});
    ClickGround({-3, 0, 3});
    REQUIRE(Core().Objects().size() == BASE + 2);
    CHECK(Core().KindOf(Core().Objects()[BASE]) == Editor::ObjectKind::Model);
    RightClickGround({0, 0, -5});
    CHECK_EQ(Gui().PlacingAsset(), std::string());
    CHECK(!Gui().Menu().IsOpen());
    ClickGround({0, 0, -5});
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));

    // Drag a prefab onto the scene: dropped where it is released
    const auto* turret = AssetRow("turret");
    REQUIRE(turret != nullptr);
    Press(turret->X + 4.0f, turret->Y + 4.0f);
    Vec2 target = ScreenOf({-4, 0, -4});
    for (int i = 1; i <= 4; ++i)
        MoveMouse(turret->X + (target.X - turret->X) * i / 4.0f, turret->Y + (target.Y - turret->Y) * i / 4.0f);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2)); // nothing until released
    Release();
    Entity root = Core().Selected();
    CHECK_EQ(Core().PrefabOf(root), std::string("turret"));
    CHECK(std::fabs(SceneObjects::GetPosition(root).X + 4.0f) < 0.3f);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 6));
    // Instances are listed in blue in the hierarchy
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(TreeRow(Core().NameOf(root)) != nullptr);
    Gui().StartPlacingModel("");

    // Press on the scene while placing: place and drag in one gesture
    Gui().StartPlacingModel("Box");
    Vec2 start = ScreenOf({6, 0, 0});
    Press(start.X, start.Y);
    Entity placed = Core().Selected();
    Vec2 end = ScreenOf({8, 0, -3});
    MoveMouse(end.X, end.Y);
    Release();
    CHECK(std::fabs(SceneObjects::GetPosition(placed).X - 8.0f) < 0.3f);
    // One undo removes it again
    std::size_t count = Core().Objects().size();
    CHECK(Core().Undo());
    CHECK_EQ(Core().Objects().size(), count - 1);
    Gui().StartPlacingModel("");
}

//-----------------------------------------------------------------------------
// Selecting, moving, the keyboard
//-----------------------------------------------------------------------------

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
    for (int i = 1; i <= 4; ++i)
    {
        Vec2 s = ScreenOf({i * 1.0f, 0, i * 0.5f});
        MoveMouse(s.X, s.Y);
    }
    Release();
    Vec3 p = SceneObjects::GetPosition(e);
    CHECK(std::fabs(p.X - 4.0f) < 0.26f);
    CHECK(std::fabs(p.Z - 2.0f) < 0.26f);
    CHECK_EQ(Core().UndoCount(), undo + 1);
    CHECK(Core().Undo());
    CHECK_EQ(SceneObjects::GetPosition(e).X, 0.0f);

    // Clicking the field selects it without moving it
    Core().Select(e);
    ClickGround({8, 0, 8});
    CHECK(Core().IsField(Core().Selected()));
}

TEST_CASE("Editor GUI: keyboard shortcuts")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {1, 0, 1});
    PressKey(App::KEY_R);
    CHECK_EQ(SceneObjects::GetYaw(e), 15.0f);
    PressKey(App::KEY_F);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    PressKey(App::KEY_X);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));
    PressKey(App::KEY_U);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    PressKey(App::KEY_Y);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));

    // Holding W pans the camera forward
    Vec2 before = ScreenOf({0, 0, 0});
    AppStub::Get().Keys[App::KEY_W] = true;
    TestEnvironment::RunFrames(10, FRAME_MS);
    AppStub::Get().Keys[App::KEY_W] = false;
    TestEnvironment::RunFrame(FRAME_MS);
    Vec2 after = ScreenOf({0, 0, 0});
    CHECK(after.Y < before.Y);
}

TEST_CASE("Editor GUI: tall objects are picked where they are seen")
{
    OpenEditor();
    Editor::PlaceSettings tall;
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

//-----------------------------------------------------------------------------
// Inspector
//-----------------------------------------------------------------------------

TEST_CASE("Editor GUI: the inspector shows the object's components as sections")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Polygon, {0, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("INSPECTOR", INSPECTOR_X) != nullptr);
    CHECK(FindText("Polygon  #", INSPECTOR_X, true) != nullptr);
    CHECK(FindText("- Transform", INSPECTOR_X) != nullptr);
    CHECK(FindText("- Shape2D", INSPECTOR_X) != nullptr);
    CHECK(FindText("- RigidBody", INSPECTOR_X) == nullptr);

    REQUIRE(ClickStepper("Size", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Width, 1.25f);
    REQUIRE(ClickStepper("Sides", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Shape2D>(e).Sides, 7);
    REQUIRE(ClickStepper("Tag ", -1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<SceneObject>(e).Tag, std::string("Light"));

    // Add Component opens a menu of what can be added
    REQUIRE(ClickButton("Add Component", INSPECTOR_X));
    REQUIRE(ClickMenu({"RigidBody"}));
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Static);
    REQUIRE(ClickStepper("Body Static", +1, INSPECTOR_X));
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Dynamic);
    REQUIRE(ClickButton("Add Component", INSPECTOR_X));
    REQUIRE(ClickMenu({"Script"}));
    std::string first = ScriptRegistry::Get().Names(false).front();
    CHECK_EQ(Core().GetScript(e), first);
    const ScriptInfo* info = ScriptRegistry::Get().Find(first);
    REQUIRE(info != nullptr && !info->Params.empty());
    const ScriptParam& param = info->Params.front();
    REQUIRE(ClickStepper(param.Name, +1, INSPECTOR_X));
    CHECK_EQ(Core().GetScriptParam(e, param.Name), param.Default + param.Step);

    // Fold a section by clicking its title
    const auto* shapeTitle = FindText("- Shape2D", INSPECTOR_X);
    REQUIRE(shapeTitle != nullptr);
    Click(shapeTitle->X + 4.0f, shapeTitle->Y + 4.0f);
    CHECK(FindText("+ Shape2D", INSPECTOR_X) != nullptr);
    CHECK(FindText("Sides", INSPECTOR_X) == nullptr);
    Click(shapeTitle->X + 4.0f, shapeTitle->Y + 4.0f);
    CHECK(FindText("Sides", INSPECTOR_X) != nullptr);

    // Remove the script (the first Remove is the RigidBody's)
    const auto* scriptTitle = FindText("- Script", INSPECTOR_X);
    REQUIRE(scriptTitle != nullptr);
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.Text == "Remove" && std::fabs(printed.Y - scriptTitle->Y) < 1.5f)
        {
            Click(printed.X + 4.0f, printed.Y + 4.0f);
            break;
        }
    }
    CHECK(Core().GetScript(e).empty());
    CHECK(SceneObjects::GetBodyType(e) == BodyType::Dynamic);

    // Duplicate / Delete buttons, toolbar Undo / Redo
    REQUIRE(ClickButton("Duplicate", INSPECTOR_X));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    Entity copy = Core().Selected();
    CHECK(ECS.GetResource<RenderConstants>()->EntityToVertexRange.count(copy) == 1);
    REQUIRE(ClickButton("Delete", INSPECTOR_X));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));
    // Deleted during Render: its geometry still left the renderer
    CHECK(ECS.GetResource<RenderConstants>()->EntityToVertexRange.count(copy) == 0);
    REQUIRE(ClickButton("Undo"));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 2));
    REQUIRE(ClickButton("Redo"));
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));
}

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
    Click(label->X + 70.0f + 20.0f, label->Y + UIText::CapHeight() * 0.5f);
    // x deletes, f duplicates, wasd pans... none of it may happen while
    // typing (the keys are also reported as held)
    AppStub::Type("fox 1 wasd");
    AppStub::Get().Keys[App::KEY_X] = true;
    AppStub::Get().Keys[App::KEY_F] = true;
    TestEnvironment::RunFrame(FRAME_MS);
    AppStub::Get().Keys[App::KEY_X] = false;
    AppStub::Get().Keys[App::KEY_F] = false;
    AppStub::Type("\r");
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(Core().Objects().size(), size_t(BASE + 1));
    CHECK_EQ(Core().NameOf(e), std::string("fox 1 wasd"));

    // Names must stay unique
    Entity other = Core().Place(Editor::ObjectKind::Rectangle, {4, 0, 4});
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(TypeInto("Name", "fox 1 wasd\r", INSPECTOR_X));
    CHECK(Core().NameOf(other) != std::string("fox 1 wasd"));
    CHECK(AppStub::WasPrinted("Can not rename"));
}

TEST_CASE("Editor GUI: a long inspector scrolls with its scrollbar")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Core().SetBody(e, BodyType::Static);
    Core().SetScript(e, "Spawner");
    for (const char* component : {"Health", "Faction", "Waypoint"})
        REQUIRE(Core().AddComponent(e, component));
    TestEnvironment::RunFrame(FRAME_MS);
    float view = (EditorStyle::PANEL_TOP - 8.0f) - (EditorStyle::PANEL_BOTTOM + 6.0f);
    REQUIRE(Gui().InspectorContentHeight() > view);
    CHECK_EQ(Gui().InspectorScroll(), 0.0f);
    // The top is shown, the bottom is not
    CHECK(FindText("Name", INSPECTOR_X) != nullptr);
    CHECK(FindText("Add Component", INSPECTOR_X) == nullptr);

    // Drag the thumb (at the top of the scrollbar) to the bottom
    float barX = APP_VIRTUAL_WIDTH - EditorStyle::SCROLLBAR_W - 6.0f + 5.0f;
    Press(barX, EditorStyle::PANEL_TOP - 12.0f);
    MoveMouse(barX, 200.0f);
    MoveMouse(barX, 20.0f);
    Release();
    CHECK(Gui().InspectorScroll() > 0.0f);
    CHECK(Near(Gui().InspectorScroll(), Gui().InspectorContentHeight() - view, 1.0f));
    CHECK(FindText("Add Component", INSPECTOR_X) != nullptr);
    CHECK(FindText("Name", INSPECTOR_X) == nullptr);
    // Every shown row is inside the panel
    for (const auto& printed : AppStub::Get().Printed)
    {
        // (the status bar's Controls button is below the inspector)
        if (printed.X >= INSPECTOR_X && printed.Y < EditorStyle::PANEL_TOP && printed.Text != "Controls")
            CHECK(printed.Y >= EditorStyle::PANEL_BOTTOM);
    }
    // Fields scrolled to still work
    REQUIRE(TypeInto("Wait s", "3\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).WaitSeconds, 3.0f);

    // Clicking the track pages back up; a new selection starts at the top
    Click(barX, EditorStyle::PANEL_TOP - 20.0f);
    CHECK(Gui().InspectorScroll() < Gui().InspectorContentHeight() - view - 1.0f);
    Core().Select(Core().Objects()[0]);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(Gui().InspectorScroll(), 0.0f);
}

TEST_CASE("Editor GUI: add components, edit their generated fields, fold and remove them")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Entity other = Core().Place(Editor::ObjectKind::Circle, {4, 0, 0});
    Core().Select(e);
    TestEnvironment::RunFrame(FRAME_MS);

    REQUIRE(ClickButton("Add Component", INSPECTOR_X));
    CHECK(FindText("Faction") != nullptr);
    REQUIRE(ClickMenu({"Health"}));
    REQUIRE(ECS.HasComponent<Health>(e));
    CHECK(FindText("- Health", INSPECTOR_X) != nullptr);

    // Widgets generated from the REFLECT block: number, check box
    REQUIRE(TypeInto("Current", "250\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Health>(e).Current, 250.0f);
    REQUIRE(TypeInto("Max", "0\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Health>(e).Max, 1.0f); // Range(1, 10000)
    const auto* invulnerable = FindText("Invuln.", INSPECTOR_X);
    REQUIRE(invulnerable != nullptr);
    Click(invulnerable->X + 78.0f, invulnerable->Y + 4.0f);
    CHECK(ECS.GetComponent<Health>(e).Invulnerable);

    // Faction: enum stepper, text, colour swatches, read only value
    REQUIRE(Core().AddComponent(e, "Faction"));
    TestEnvironment::RunFrame(FRAME_MS);
    // Fold the shape's sections so Faction is in view
    for (const char* title : {"- Shader", "- Shape2D"})
    {
        if (const auto* t = FindText(title, INSPECTOR_X))
            Click(t->X + 4.0f, t->Y + 4.0f);
        TestEnvironment::RunFrame(FRAME_MS);
    }
    REQUIRE(ClickStepper("Side Neutral", 1, INSPECTOR_X));
    CHECK(ECS.GetComponent<Faction>(e).Side == Team::Player);
    REQUIRE(TypeInto("Title", "Blue team\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Faction>(e).Title, std::string("Blue team"));
    CHECK(FindText("Kills", INSPECTOR_X) != nullptr);
    const auto* banner = FindText("Banner", INSPECTOR_X);
    REQUIRE(banner != nullptr);
    Click(banner->X + 72.0f, banner->Y + 4.0f);
    CHECK(!(ECS.GetComponent<Faction>(e).Banner == Vec3(0.85f, 0.85f, 0.85f)));

    // Fold Health, then remove it
    const auto* health = FindText("- Health", INSPECTOR_X);
    REQUIRE(health != nullptr);
    Click(health->X + 4.0f, health->Y + 4.0f);
    CHECK(FindText("+ Health", INSPECTOR_X) != nullptr);
    CHECK(FindText("Current", INSPECTOR_X) == nullptr);
    health = FindText("+ Health", INSPECTOR_X);
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.Text == "Remove" && std::fabs(printed.Y - health->Y) < 1.5f)
        {
            Click(printed.X + 4.0f, printed.Y + 4.0f);
            break;
        }
    }
    CHECK(!ECS.HasComponent<Health>(e));
    CHECK(ECS.HasComponent<Faction>(e));
    CHECK(AppStub::WasPrinted("Removed Health"));
    REQUIRE(Core().Undo());
    CHECK(ECS.HasComponent<Health>(e));
    // Unfold for the next tests
    TestEnvironment::RunFrame(FRAME_MS);
    health = FindText("+ Health", INSPECTOR_X);
    REQUIRE(health != nullptr);
    Click(health->X + 4.0f, health->Y + 4.0f);

    // Waypoint: type the name of the object to point at; tooltip on hover
    REQUIRE(Core().AddComponent(e, "Waypoint"));
    // Fold the others so Waypoint is in view
    for (const char* title : {"- Health", "- Faction", "- Shape2D"})
    {
        TestEnvironment::RunFrame(FRAME_MS);
        const auto* t = FindText(title, INSPECTOR_X);
        if (t != nullptr)
            Click(t->X + 4.0f, t->Y + 4.0f);
    }
    REQUIRE(TypeInto("Next", Core().NameOf(other) + "\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).Next, other);
    REQUIRE(TypeInto("Next", "nobody\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<Waypoint>(e).Next, other);
    CHECK(AppStub::WasPrinted("No object named nobody"));
    const auto* next = FindText("Next", INSPECTOR_X);
    REQUIRE(next != nullptr);
    MoveMouse(next->X + 5.0f, next->Y);
    CHECK(AppStub::WasPrinted("Next: Object to go to next"));
    // Unfold everything again
    for (const char* title : {"+ Health", "+ Faction", "+ Shape2D"})
    {
        TestEnvironment::RunFrame(FRAME_MS);
        const auto* t = FindText(title, INSPECTOR_X);
        if (t != nullptr)
            Click(t->X + 4.0f, t->Y + 4.0f);
    }
}

TEST_CASE("Editor GUI: the scene settings set the scene script and the field")
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
    CHECK(FindText("INSPECTOR", INSPECTOR_X) != nullptr);
}

//-----------------------------------------------------------------------------
// Play
//-----------------------------------------------------------------------------

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
    CHECK(FindText("HIERARCHY") == nullptr);

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

//-----------------------------------------------------------------------------
// Scene documents
//-----------------------------------------------------------------------------

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
    // The field, the camera and the light
    CHECK_EQ(Core().Objects().size(), BASE);

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

//-----------------------------------------------------------------------------
// Hierarchy
//-----------------------------------------------------------------------------

TEST_CASE("Editor GUI: the hierarchy tree shows parents and children, drag rows to parent them")
{
    OpenEditor();
    Entity rect = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Entity circle = Core().Place(Editor::ObjectKind::Circle, {4, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
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
    const auto* fold = FindText("-");
    REQUIRE(fold != nullptr);
    CHECK(fold->X < rectRow->X);
    Click(fold->X + 2.0f, fold->Y + 4.0f);
    CHECK(TreeRow("Circle") == nullptr);
    fold = FindText("+", -1.0f);
    REQUIRE(fold != nullptr);
    // The first "+" is the title's create button: the fold is on Rectangle's row
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.Text == "+" && std::fabs(printed.Y - TreeRow("Rectangle")->Y) < 3.0f)
            fold = &printed;
    }
    Click(fold->X + 2.0f, fold->Y + 4.0f);
    REQUIRE(TreeRow("Circle") != nullptr);

    // Drop Circle on SCENE: top level again
    const auto* scene = FindText("SCENE");
    DragRow(*TreeRow("Circle"), scene->X + 10.0f, scene->Y + 4.0f);
    CHECK_EQ(Core().ParentOf(circle), NULL_ENTITY);
    CHECK(AppStub::WasPrinted("Circle is now a top level object"));
    REQUIRE(Core().Undo());
    CHECK_EQ(Core().ParentOf(circle), rect);
}

TEST_CASE("Editor GUI: empties are drawn as crosses, the Parent field")
{
    OpenEditor();
    Entity rect = Core().Place(Editor::ObjectKind::Rectangle, {-4, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    RightClickGround({-4, 0, 0});
    REQUIRE(ClickMenu({"Create Child", "Create Empty"}));
    Entity empty = Core().Selected();
    REQUIRE(SceneObjects::IsEmpty(empty));
    CHECK_EQ(Core().ParentOf(empty), rect);
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
    // Clicking the cross selects it
    Vec2 s = ScreenOf(SceneObjects::GetPosition(empty));
    Click(s.X, s.Y);
    CHECK_EQ(Core().Selected(), empty);

    // The Transform section's Parent field: type a name ("-" = top level)
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(TypeInto("Parent", "-\r", INSPECTOR_X));
    CHECK_EQ(Core().ParentOf(empty), NULL_ENTITY);
    REQUIRE(TypeInto("Parent", "nobody\r", INSPECTOR_X));
    CHECK(AppStub::WasPrinted("No object named nobody"));
    REQUIRE(TypeInto("Parent", "Rectangle\r", INSPECTOR_X));
    CHECK_EQ(Core().ParentOf(empty), rect);
    // Empties have no shape, only a scale
    CHECK(FindText("Scale", INSPECTOR_X) != nullptr);
    CHECK(FindText("- Shape2D", INSPECTOR_X) == nullptr);
}

//-----------------------------------------------------------------------------
// Prefab editor
//-----------------------------------------------------------------------------

TEST_CASE("Editor GUI: save a group as a prefab, edit it, the scene's instances follow")
{
    PrefabFolder prefabs;
    OpenEditor();
    Entity tower = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Core().Rename(tower, "Tower");
    Core().Create(Editor::ObjectKind::Circle, {1, 0, 0}, tower);
    TestEnvironment::RunFrame(FRAME_MS);

    // Save as Prefab from the object's menu: a file, and the object is an instance
    RightClickGround({0, 0, 0});
    REQUIRE(ClickMenu({"Save as Prefab"}));
    CHECK(prefabs.Exists("Tower"));
    CHECK_EQ(Core().PrefabOf(tower), std::string("Tower"));
    CHECK(AssetRow("Tower") != nullptr);
    CHECK(FindText("- Prefab", INSPECTOR_X) != nullptr);
    // A second instance from the Assets list
    Gui().StartPlacingPrefab("Tower");
    ClickGround({6, 0, 6});
    Gui().StartPlacingPrefab("");
    Entity second = Core().Selected();
    CHECK_EQ(Core().PrefabOf(second), std::string("Tower"));
    std::size_t objects = Core().Objects().size();

    // Edit Prefab: the prefab alone on a stage
    RightClickGround({0, 0, 0});
    REQUIRE(ClickMenu({"Edit Prefab"}));
    REQUIRE(Gui().InPrefabMode());
    CHECK(FindText("Save Prefab") != nullptr);
    CHECK(FindText("Back to Scene") != nullptr);
    CHECK(FindText("New") == nullptr);
    CHECK_EQ(Core().Objects().size(), size_t(3)); // field, Tower, its circle
    Entity root = Core().RootObjects()[1];
    CHECK_EQ(Core().PrefabOf(root), std::string());

    // Add a child, leaving without saving only warns
    RightClickGround({0, 0, 0});
    REQUIRE(ClickMenu({"Create Child", "Create Triangle"}));
    REQUIRE(ClickButton("Back to Scene"));
    CHECK(Gui().InPrefabMode());
    CHECK(AppStub::WasPrinted("unsaved changes"));
    REQUIRE(ClickButton("Save Prefab"));
    CHECK(!Core().IsDirty());
    REQUIRE(ClickButton("Back to Scene"));
    CHECK(!Gui().InPrefabMode());

    // Both instances got the new child
    CHECK_EQ(Core().Objects().size(), objects + 2);
    for (Entity e : Core().Objects())
    {
        if (Core().PrefabOf(e) == "Tower")
            CHECK_EQ(Core().ChildrenOf(e).size(), size_t(2));
    }
    CHECK(AppStub::WasPrinted("updated 2 instance(s) of Tower"));
    // One undo step puts the old instances back
    REQUIRE(Core().Undo());
    CHECK_EQ(Core().Objects().size(), objects);

    // Unpack from the inspector's Prefab section
    // (the field, the camera and the light come first)
    Core().Select(Core().RootObjects()[BASE]);
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickButton("Unpack", INSPECTOR_X));
    CHECK_EQ(Core().PrefabOf(Core().RootObjects()[BASE]), std::string());
}

TEST_CASE("Editor GUI: New Prefab starts an empty prefab stage")
{
    PrefabFolder prefabs;
    OpenEditor();
    Entity keep = Core().Place(Editor::ObjectKind::Circle, {3, 0, 3});
    Fixture::WorldImage scene = Fixture::Capture();
    REQUIRE(ClickButton("New Prefab"));
    REQUIRE(Gui().InPrefabMode());
    const std::string name = Gui().PrefabName();
    CHECK_EQ(name, std::string("prefab"));
    // Its root, ready for children
    REQUIRE(Core().RootObjects().size() == 2u);
    Entity root = Core().RootObjects()[1];
    CHECK_EQ(Core().NameOf(root), name);
    CHECK(FindText("PREFAB " + name) != nullptr);
    const auto* row = TreeRow(name);
    REQUIRE(row != nullptr);
    RightClick(row->X + 4.0f, row->Y + 4.0f);
    REQUIRE(ClickMenu({"Create Child", "Create Rectangle"}));
    REQUIRE(ClickButton("Save Prefab"));
    CHECK(prefabs.Exists(name));
    REQUIRE(ClickButton("Back to Scene"));
    // The scene is back exactly as it was
    CHECK_SAME_WORLD(scene, Fixture::Capture());
    CHECK(Core().IsObject(keep));
    CHECK(AssetRow(name) != nullptr);
}

//-----------------------------------------------------------------------------
// Responsive layout
//-----------------------------------------------------------------------------

namespace
{
    // Every text is inside the panel it is drawn in, and button labels are
    // centered in their button
    void CheckTextStaysInside(const std::string& when)
    {
        for (const auto& printed : AppStub::Get().Printed)
        {
            float right = EditorStyle::PanelRight(printed.X, printed.Y);
            float end = printed.X + UIText::Width(printed.Text);
            ++TestFramework::TotalChecks();
            if (printed.X < -0.5f || end > right + 4.5f)
                TestFramework::ReportFailure(__FILE__, __LINE__,
                                             when + ": '" + printed.Text + "' goes out of its panel");
        }
    }
} // namespace

TEST_CASE("Editor GUI: text stays centered and inside its widgets at any window size")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {0, 0, 0});
    Core().Rename(e, "A rather long object name that does not fit anywhere");
    Core().SetScript(e, "Spawner");
    REQUIRE(Core().AddComponent(e, "Faction"));

    const int sizes[][2] = {{1024, 768}, {2048, 1536}, {1920, 1080}, {800, 600}, {640, 480}};
    for (const auto& size : sizes)
    {
        UIText::SetWindowSize(size[0], size[1]);
        TestEnvironment::RunFrame(FRAME_MS);
        std::string when = std::to_string(size[0]) + "x" + std::to_string(size[1]);
        CheckTextStaysInside(when);

        // The toolbar's New button starts right of the left panel; its label
        // is centered in it
        const auto* label = FindText("New");
        REQUIRE(label != nullptr);
        float buttonW = std::max(50.0f, UIText::Width("New") + 16.0f);
        CHECK(Near(label->X + UIText::Width("New") * 0.5f, LEFT_W + buttonW * 0.5f, 0.5f));
        // Vertically centered on the toolbar button too
        float buttonY = APP_VIRTUAL_HEIGHT - EditorStyle::TOOLBAR_H + (EditorStyle::TOOLBAR_H - EditorStyle::BUTTON_H) * 0.5f;
        CHECK(Near(label->Y + UIText::CapHeight() * 0.5f, buttonY + EditorStyle::BUTTON_H * 0.5f, 0.5f));

        // The context menu stays on the screen, even opened at the corner
        RightClick(APP_VIRTUAL_WIDTH - EditorStyle::INSPECTOR_W - 5.0f, EditorStyle::STATUS_H + 5.0f);
        REQUIRE(Gui().Menu().IsOpen());
        for (const std::string& item : Gui().Menu().Labels())
        {
            // Labels wider than a third of the screen are cut ("Long..")
            const AppStub::State::PrintedText* printed = nullptr;
            for (const auto& p : AppStub::Get().Printed)
            {
                bool cut = p.Text.size() > 2 && p.Text.compare(p.Text.size() - 2, 2, "..") == 0 &&
                           item.rfind(p.Text.substr(0, p.Text.size() - 2), 0) == 0;
                if (p.Text == item || cut)
                    printed = &p;
            }
            REQUIRE(printed != nullptr);
            CHECK(printed->X >= 0.0f);
            CHECK(printed->X + UIText::Width(printed->Text) <= APP_VIRTUAL_WIDTH);
            CHECK(printed->Y >= 0.0f);
        }
        Gui().Menu().Close();
    }
    UIText::SetWindowSize(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT);
    TestEnvironment::RunFrame(FRAME_MS);
}

//-----------------------------------------------------------------------------
// The editor's view (its own camera) and the game camera object
//-----------------------------------------------------------------------------

namespace
{
    void HoldKey(App::Key key, int frames)
    {
        AppStub::Get().Keys[key] = true;
        TestEnvironment::RunFrames(frames, FRAME_MS);
        AppStub::Get().Keys[key] = false;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    bool NearVec(const Vec3& a, const Vec3& b, float eps = 1e-3f)
    {
        return Near(a.X, b.X, eps) && Near(a.Y, b.Y, eps) && Near(a.Z, b.Z, eps);
    }
} // namespace

TEST_CASE("Editor GUI: the view orbits, tilts, rises and resets without touching the game camera")
{
    OpenEditor();
    Gui().SetEditorView(SceneEditorScene::DefaultView());
    Entity camera = Core().GameCameraObject();
    REQUIRE(camera != NULL_ENTITY);
    SceneCamera::View game = SceneCamera::ViewOf(camera);
    auto renderer = ECS.GetResource<Camera>();

    // Right arrow orbits (yaw), Up tilts (pitch), E / V rise / sink
    HoldKey(App::KEY_RIGHT, 10);
    float yaw = Gui().EditorView().Yaw;
    CHECK(yaw > 5.0f);
    HoldKey(App::KEY_LEFT, 20);
    CHECK(Gui().EditorView().Yaw > 180.0f); // wrapped below 0
    float pitch = Gui().EditorView().Pitch;
    HoldKey(App::KEY_DOWN, 10);
    CHECK(Gui().EditorView().Pitch < pitch);
    HoldKey(App::KEY_UP, 400);
    CHECK_EQ(Gui().EditorView().Pitch, 89.0f); // clamped
    HoldKey(App::KEY_E, 10);
    CHECK(Gui().EditorView().Target.Y > 0.5f);
    float height = Gui().EditorView().Target.Y;
    HoldKey(App::KEY_V, 5);
    CHECK(Gui().EditorView().Target.Y < height);
    // The renderer shows the editor's view
    CHECK(NearVec(renderer->Position, SceneCamera::EyeOf(Gui().EditorView()), 0.01f));
    // ... and the game camera did not move
    CHECK(SceneCamera::ViewOf(camera) == game);

    // WASD pans relative to where the view looks: looking along +X, W goes +X
    SceneCamera::View side = SceneEditorScene::DefaultView();
    side.Yaw = 90.0f;
    Gui().SetEditorView(side);
    HoldKey(App::KEY_W, 10);
    CHECK(Gui().EditorView().Target.X > 1.0f);
    CHECK(Near(Gui().EditorView().Target.Z, 0.0f, 0.01f));

    // Home resets the view
    PressKey(App::KEY_HOME);
    CHECK(Gui().EditorView() == SceneEditorScene::DefaultView());
}

TEST_CASE("Editor GUI: Play looks through the game camera, Stop brings the editor's view back")
{
    OpenEditor();
    SceneCamera::View editorView = SceneEditorScene::DefaultView();
    editorView.Yaw = 200.0f;
    editorView.Distance = 12.0f;
    Gui().SetEditorView(editorView);
    SceneCamera::View game;
    game.Target = {3, 0, 3};
    game.Distance = 20.0f;
    Core().SetGameCamera(game);
    auto renderer = ECS.GetResource<Camera>();

    PressKey(App::KEY_P);
    REQUIRE(Core().IsPlaying());
    CHECK(NearVec(renderer->Position, SceneCamera::EyeOf(SceneCamera::Current()), 0.01f));
    // The editor's camera keys do nothing while playing
    HoldKey(App::KEY_RIGHT, 5);
    CHECK_EQ(Gui().EditorView().Yaw, 200.0f);
    PressKey(App::KEY_P);
    REQUIRE(!Core().IsPlaying());
    CHECK(NearVec(renderer->Position, SceneCamera::EyeOf(Gui().EditorView()), 0.01f));
    CHECK_EQ(Gui().EditorView().Yaw, 200.0f);
}

TEST_CASE("Editor GUI: the game camera is in the hierarchy, drawn in the view and edited like an object")
{
    OpenEditor();
    Gui().SetEditorView(SceneEditorScene::DefaultView());
    // Zoomed out and turned so the camera's eye is in front of the view
    SceneCamera::View away = SceneEditorScene::DefaultView();
    away.Yaw = 180.0f;
    away.Distance = 60.0f;
    Gui().SetEditorView(away);
    TestEnvironment::RunFrame(FRAME_MS);
    Entity camera = Core().GameCameraObject();
    REQUIRE(TreeRow("Main Camera") != nullptr);
    // Its gizmo: lines and its name near the eye
    CHECK(!AppStub::Get().Lines.empty());
    Vec2 eye = ScreenOf(SceneCamera::EyeOf(SceneCamera::ViewOf(camera)));
    const auto* label = FindText("Main Camera", LEFT_W);
    REQUIRE(label != nullptr);
    CHECK(Near(label->X, eye.X + 8.0f, 1.0f));

    // Select it by its hierarchy row: the inspector shows its GameCamera
    const auto* row = TreeRow("Main Camera");
    Click(row->X + 4.0f, row->Y + 4.0f);
    CHECK_EQ(Core().Selected(), camera);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("Game camera", INSPECTOR_X, true) != nullptr);
    if (const auto* folded = FindText("+ GameCamera", INSPECTOR_X))
        Click(folded->X + 4.0f, folded->Y + 4.0f);
    REQUIRE(FindText("- GameCamera", INSPECTOR_X) != nullptr);
    REQUIRE(TypeInto("Dist.", "45\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<GameCamera>(camera).Distance, 45.0f);
    REQUIRE(ClickStepper("FOV", +1, INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<GameCamera>(camera).FieldOfView, 95.0f);
    // Moved and turned like any object
    PressKey(App::KEY_R);
    CHECK(Near(SceneObjects::GetYaw(camera), 15.0f));
    REQUIRE(TypeInto("Pos X", "4\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(camera).X, 4.0f);

    // Its menu: View Through Camera, then Align with View
    Gui().Menu().Open(300.0f, 400.0f, Gui().ObjectItems(camera));
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickMenu({"View Through Camera"}));
    CHECK(NearVec(Gui().EditorView().Target, SceneObjects::GetPosition(camera)));
    CHECK(Near(Gui().EditorView().Yaw, 15.0f, 0.01f));
    SceneCamera::View moved = Gui().EditorView();
    moved.Target = {-2, 0, -2};
    Gui().SetEditorView(moved);
    Gui().Menu().Open(300.0f, 400.0f, Gui().ObjectItems(camera));
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickMenu({"Align with View"}));
    CHECK(NearVec(SceneObjects::GetPosition(camera), Vec3(-2, 0, -2)));
    // Keeps its own lens
    CHECK_EQ(ECS.GetComponent<GameCamera>(camera).FieldOfView, 95.0f);

    // Create Camera from the ground menu
    RightClickGround({5, 0, 5});
    REQUIRE(ClickMenu({"Create Camera"}));
    CHECK(Core().IsCamera(Core().Selected()));
    CHECK_EQ(Core().GameCameraObject(), camera);
    Gui().SetEditorView(SceneEditorScene::DefaultView());
}

TEST_CASE("Editor GUI: raise, lower and turn the selection from the keyboard and the inspector")
{
    OpenEditor();
    Gui().SetEditorView(SceneEditorScene::DefaultView());
    Entity e = Core().Place(Editor::ObjectKind::Rectangle, {2, 0, 2});
    Core().Select(e);
    TestEnvironment::RunFrame(FRAME_MS);
    PressKey(App::KEY_I);
    PressKey(App::KEY_I);
    CHECK_EQ(SceneObjects::GetPosition(e).Y, 1.0f);
    PressKey(App::KEY_K);
    CHECK_EQ(SceneObjects::GetPosition(e).Y, 0.5f);
    CHECK(AppStub::WasPrinted("height 0.50"));
    PressKey(App::KEY_L);
    CHECK_EQ(SceneObjects::GetYaw(e), 15.0f);
    PressKey(App::KEY_J);
    PressKey(App::KEY_J);
    CHECK_EQ(SceneObjects::GetYaw(e), 345.0f);
    // Pos Y in the Transform section
    REQUIRE(TypeInto("Pos Y", "3\r", INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).Y, 3.0f);
    REQUIRE(ClickStepper("Pos Y", -1, INSPECTOR_X));
    CHECK_EQ(SceneObjects::GetPosition(e).Y, 2.5f);
    // Moving keeps the height, undo takes it back
    Core().Move(e, {0, 0, 0});
    CHECK_EQ(SceneObjects::GetPosition(e).Y, 2.5f);
    REQUIRE(Core().Undo());
    REQUIRE(Core().Undo());
    CHECK_EQ(SceneObjects::GetPosition(Core().Objects().back()).Y, 3.0f);
}

TEST_CASE("Editor GUI: the Shader section picks the fragment and vertex shaders")
{
    OpenEditor();
    Entity e = Core().Place(Editor::ObjectKind::Circle, {0, 0, 0});
    Core().Select(e);
    TestEnvironment::RunFrame(FRAME_MS);
    if (const auto* folded = FindText("+ Shader", INSPECTOR_X))
        Click(folded->X + 4.0f, folded->Y + 4.0f);
    // Fold the sections above it so it is in view
    for (const char* title : {"- Transform", "- Shape2D"})
    {
        if (const auto* t = FindText(title, INSPECTOR_X))
            Click(t->X + 4.0f, t->Y + 4.0f);
        TestEnvironment::RunFrame(FRAME_MS);
    }
    CHECK(FindText("Shape", INSPECTOR_X) != nullptr);
    REQUIRE(ClickStepper("Frag Shape", +1, INSPECTOR_X));
    CHECK(SceneObjects::FragmentShaderOf(e) == BlinnPhongID);
    REQUIRE(ClickStepper("Frag Lit", -1, INSPECTOR_X));
    REQUIRE(ClickStepper("Frag Shape", -1, INSPECTOR_X));
    CHECK(SceneObjects::FragmentShaderOf(e) == RedShaderID); // wraps to the last listed one
    REQUIRE(ClickStepper("Vert Default", +1, INSPECTOR_X));
    CHECK(SceneObjects::VertexShaderOf(e) == WaveVertShaderID);
    CHECK(AppStub::WasPrinted("Vertex shader Wave"));
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("Red + Wave", INSPECTOR_X) != nullptr);
    // Unfold for the next tests
    for (const char* title : {"+ Transform", "+ Shape2D"})
    {
        TestEnvironment::RunFrame(FRAME_MS);
        if (const auto* t = FindText(title, INSPECTOR_X))
            Click(t->X + 4.0f, t->Y + 4.0f);
    }
}

TEST_CASE("Editor GUI: the Controls panel lists every control")
{
    OpenEditor();
    CHECK(!Gui().ControlsOpen());
    // H opens it
    PressKey(App::KEY_H);
    REQUIRE(Gui().ControlsOpen());
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("CONTROLS") != nullptr);
    for (const auto& group : SceneEditorScene::Controls())
    {
        CHECK(FindText(group.Title) != nullptr);
        for (const auto& control : group.Controls)
        {
            ++TestFramework::TotalChecks();
            if (FindText(control.Keys) == nullptr || FindText(control.Action) == nullptr)
                TestFramework::ReportFailure(__FILE__, __LINE__,
                                             std::string("control cut or missing: ") + control.Keys + " / " + control.Action);
        }
    }
    // Every text stays inside the panel
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.X > LEFT_W && printed.X < INSPECTOR_X && printed.Y > EditorStyle::STATUS_H &&
            printed.Y < EditorStyle::PANEL_TOP)
            CHECK(printed.X + UIText::Width(printed.Text) <= INSPECTOR_X);
    }
    // Clicks on the panel do not reach the scene
    Entity selected = Core().Selected();
    ClickGround({0, 0, 0});
    CHECK_EQ(Core().Selected(), selected);
    // Close button
    REQUIRE(ClickButton("Close"));
    CHECK(!Gui().ControlsOpen());
    // The status bar's Controls button, then Esc
    REQUIRE(ClickButton("Controls"));
    CHECK(Gui().ControlsOpen());
    AppStub::Type("\x1b");
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(!Gui().ControlsOpen());
    // The status bar points to it
    CHECK(AppStub::WasPrinted("H: all controls"));
}

TEST_CASE("Editor GUI: the light is in the hierarchy, drawn in the view and edited like an object")
{
    OpenEditor();
    // Turned and zoomed out so the light (high above the field) is in view
    SceneCamera::View away = SceneEditorScene::DefaultView();
    away.Yaw = 180.0f;
    away.Distance = 70.0f;
    away.Pitch = 30.0f;
    Gui().SetEditorView(away);
    TestEnvironment::RunFrame(FRAME_MS);
    Entity light = Core().LightObject();
    REQUIRE(light != NULL_ENTITY);
    REQUIRE(TreeRow("Directional Light") != nullptr);
    // Its gizmo: its name next to the sun marker, and orange lines
    Vec2 at = ScreenOf(SceneObjects::GetPosition(light));
    const auto* label = FindText("Directional Light", LEFT_W);
    REQUIRE(label != nullptr);
    CHECK(Near(label->X, at.X + 12.0f, 1.0f));
    std::size_t orange = 0;
    for (const auto& line : AppStub::Get().Lines)
    {
        if (Near(line.R, EditorStyle::LIGHT_COLOR.R, 0.01f) && Near(line.G, EditorStyle::LIGHT_COLOR.G, 0.01f))
            ++orange;
    }
    CHECK(orange >= 12);

    // Clicking the sun marker selects it
    Click(at.X, at.Y);
    CHECK_EQ(Core().Selected(), light);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(FindText("Light  #", INSPECTOR_X, true) != nullptr);
    // Its SceneLight section
    if (const auto* folded = FindText("+ SceneLight", INSPECTOR_X))
        Click(folded->X + 4.0f, folded->Y + 4.0f);
    for (const char* title : {"- Transform"})
    {
        if (const auto* t = FindText(title, INSPECTOR_X))
            Click(t->X + 4.0f, t->Y + 4.0f);
        TestEnvironment::RunFrame(FRAME_MS);
    }
    REQUIRE(FindText("- SceneLight", INSPECTOR_X) != nullptr);
    // Every field is shown with its whole label
    for (const char* field : {"Type Directional", "Color", "Power", "Ambient", "Pitch", "Spread", "Shadow"})
        CHECK(FindText(field, INSPECTOR_X) != nullptr);
    REQUIRE(TypeInto("Power", "1.5\r", INSPECTOR_X));
    CHECK_EQ(ECS.GetComponent<SceneLight>(light).Intensity, 1.5f);
    REQUIRE(ClickStepper("Pitch", -1, INSPECTOR_X));
    CHECK(Near(ECS.GetComponent<SceneLight>(light).Pitch, 73.69f, 0.01f));
    // The renderer's light follows at once
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(ECS.GetResource<Lighting>()->GetDirectionalLight().Intensity, 1.5f);
    if (const auto* t = FindText("+ Transform", INSPECTOR_X))
        Click(t->X + 4.0f, t->Y + 4.0f);

    // Raised with I, like any object
    PressKey(App::KEY_I);
    CHECK_EQ(SceneObjects::GetPosition(light).Y, 25.5f);

    // Menus: aim it at the view's centre; aim it at an object; create one
    Gui().SetEditorView(SceneEditorScene::DefaultView());
    Gui().Menu().Open(300.0f, 400.0f, Gui().ObjectItems(light));
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickMenu({"Aim at View Center"}));
    CHECK(NearVec(SceneLighting::GroundTarget(SceneLighting::Current()), Vec3(0, 0, 0), 0.01f));
    Entity box = Core().Place(Editor::ObjectKind::Rectangle, {6, 0, 4});
    Gui().Menu().Open(300.0f, 400.0f, Gui().ObjectItems(box));
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ClickMenu({"Aim Light Here"}));
    CHECK(NearVec(SceneLighting::GroundTarget(SceneLighting::Current()), Vec3(6, 0, 4), 0.01f));
    CHECK(AppStub::WasPrinted("now shines at"));
    RightClickGround({-5, 0, -5});
    REQUIRE(ClickMenu({"Create Light"}));
    CHECK(Core().IsLight(Core().Selected()));
    CHECK_EQ(Core().LightObject(), light);
}
