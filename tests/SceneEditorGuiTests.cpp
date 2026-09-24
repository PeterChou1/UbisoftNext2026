//---------------------------------------------------------------------------------
// SceneEditorGuiTests.cpp
//---------------------------------------------------------------------------------
//
// Drives the real scene editor GUI (SceneEditorScene) headlessly: the App API
// is stubbed (tests/support/AppStub.cpp), frames are simulated the way the
// GameManager runs them, and the tests click buttons by their label, click and
// drag in the 3D viewport and press keys. This checks the wiring the unit
// tests of the editor core can not: widget hit testing, mouse to ground ray
// casting, dragging, shortcuts, file slots and play testing.
//
#include "AppStub.h"
#include "Camera.h"
#include "Editor/SceneEditorScene.h"
#include "GameManager.h"
#include "GameWorldFixture.h"
#include "IndexBuffer.h"
#include "Lighting.h"
#include "RenderConstants.h"
#include "UIStateManager.h"
#include "VertexBuffer.h"

#include <cmath>
#include <filesystem>

// -- Minimal game environment for the editor scene ------------------------------

GameManager GameSceneManager;

namespace
{
    std::vector<std::string> g_RequestedLoads;
    std::vector<std::string> g_SceneSwitches;
    std::vector<std::string> g_PlaytestReturnScenes;
} // namespace

// The editor only needs these GameManager entry points
void GameManager::RequestLoad(const std::string& path)
{
    g_RequestedLoads.push_back(path);
}

void GameManager::SetActiveScene(const std::string& sceneName)
{
    g_SceneSwitches.push_back(sceneName);
}

void GameManager::BeginPlaytest(const std::string& path, const std::string& returnScene)
{
    g_PlaytestReturnScenes.push_back(returnScene);
    RequestLoad(path);
}

// Asset loading needs the renderer, not relevant for these tests
void LoadMainLevelAssets() {}

namespace
{
    namespace fs = std::filesystem;
    using Editor::EntityKind;

    constexpr float FRAME_MS = 16.0f;

    /**
     * \brief Runs the editor in a temporary working directory (scene files are
     *        written relative to it) and simulates frames
     */
    class GuiHarness
    {
      public:
        GuiHarness()
        {
            m_OldDirectory = fs::current_path();
            m_Directory = fs::temp_directory_path() / "ubisoft_next_editor_gui";
            fs::remove_all(m_Directory);
            fs::create_directories(m_Directory / "data" / "scenes");
            fs::current_path(m_Directory);

            Fixture::FreshWorld();
            RegisterResource<Camera>();
            RegisterResource<Lighting>();
            RegisterResource<GameOptions>();
            RegisterResource<VertexBuffer>();
            RegisterResource<IndexBuffer>();
            RegisterResource<RenderConstants>();
            AppStub::Reset();
            g_RequestedLoads.clear();
            g_SceneSwitches.clear();
            g_PlaytestReturnScenes.clear();

            m_Input = std::make_unique<UIStateManager>();
            m_Scene.Start();
            m_Scene.Setup();
            Frame();
        }

        ~GuiHarness()
        {
            fs::current_path(m_OldDirectory);
            std::error_code ec;
            fs::remove_all(m_Directory, ec);
        }

        GuiHarness(const GuiHarness&) = delete;
        GuiHarness& operator=(const GuiHarness&) = delete;

        /**
         * \brief Leave and re-enter the editor like GameManager::SetActiveScene
         */
        void ReenterScene()
        {
            ECS.Reset();
            m_Scene.Setup();
            Frame();
        }

        /**
         * \brief One frame, in the same order as GameManager::Update / Render
         */
        void Frame()
        {
            AppStub::Get().Printed.clear();
            m_Input->Update();
            m_Scene.Update(FRAME_MS);
            m_Scene.Render();
            m_Input->CleanUp();
            ECS.FlushECS();
        }

        void MoveMouse(float x, float y)
        {
            AppStub::Get().MouseX = x;
            AppStub::Get().MouseY = y;
        }

        /**
         * \brief Press and release the left button at (x, y)
         */
        void Click(float x, float y)
        {
            MoveMouse(x, y);
            AppStub::Get().LeftDown = true;
            Frame();
            AppStub::Get().LeftDown = false;
            Frame();
        }

        void RightClick(float x, float y)
        {
            MoveMouse(x, y);
            AppStub::Get().RightDown = true;
            Frame();
            AppStub::Get().RightDown = false;
            Frame();
        }

        /**
         * \brief Click the widget whose label is exactly `label`
         */
        bool ClickLabel(const std::string& label)
        {
            for (const auto& printed : AppStub::Get().Printed)
            {
                if (printed.Text == label)
                {
                    // Labels are drawn centred inside their button
                    Click(printed.X + 3.0f, printed.Y + 3.0f);
                    return true;
                }
            }
            return false;
        }

        /**
         * \brief Click the [-] or [+] button of the stepper row starting with `prefix`
         */
        bool ClickStepper(const std::string& prefix, int direction)
        {
            const AppStub::State::PrintedText* row = AppStub::FindPrinted(prefix);
            if (row == nullptr)
                return false;
            // Row layout (SceneEditorScene::Stepper): text at y + 7, buttons end
            // at the right edge of the panel (20 px padding)
            float panelRight = row->X < 170.0f ? 160.0f : APP_VIRTUAL_WIDTH - 10.0f;
            float plusCenter = panelRight - 13.0f;
            float minusCenter = plusCenter - 30.0f;
            Click(direction > 0 ? plusCenter : minusCenter, row->Y - 7.0f + 11.0f);
            return true;
        }

        void PressKey(App::Key key)
        {
            AppStub::Get().Keys[key] = true;
            Frame();
            AppStub::Get().Keys[key] = false;
            Frame();
        }

        void HoldKey(App::Key key, int frames)
        {
            AppStub::Get().Keys[key] = true;
            for (int i = 0; i < frames; ++i)
                Frame();
            AppStub::Get().Keys[key] = false;
            Frame();
        }

        Vec2 ScreenOf(const Vec3& world)
        {
            return ECS.GetResource<Camera>()->WorldPointToScreenSpace(world);
        }

        void ClickWorld(const Vec3& world)
        {
            Vec2 s = ScreenOf(world);
            Click(s.X, s.Y);
        }

        void DragWorld(const Vec3& from, const Vec3& to, int steps = 5)
        {
            Vec2 a = ScreenOf(from);
            Vec2 b = ScreenOf(to);
            MoveMouse(a.X, a.Y);
            AppStub::Get().LeftDown = true;
            Frame();
            for (int i = 1; i <= steps; ++i)
            {
                float t = static_cast<float>(i) / steps;
                MoveMouse(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t);
                Frame();
            }
            AppStub::Get().LeftDown = false;
            Frame();
        }

        std::vector<Entity> ObjectsOfKind(EntityKind kind)
        {
            std::vector<Entity> result;
            for (Entity e : Probe.EditableEntities())
            {
                if (Probe.KindOf(e) == kind)
                    result.push_back(e);
            }
            return result;
        }

        // Stateless queries (KindOf, positions ...) about the shared ECS world
        Editor::SceneEditor Probe;

      private:
        template <typename T>
        void RegisterResource()
        {
            if (!ECS.HasResource<T>())
                ECS.RegisterResource(T());
        }

        SceneEditorScene m_Scene;
        std::unique_ptr<UIStateManager> m_Input;
        fs::path m_Directory;
        fs::path m_OldDirectory;
    };

    float DistanceXZ(const Vec3& a, const Vec3& b)
    {
        return std::sqrt((a.X - b.X) * (a.X - b.X) + (a.Z - b.Z) * (a.Z - b.Z));
    }
} // namespace

TEST_CASE("Editor GUI: opens with an empty playable scene and draws its panels")
{
    GuiHarness gui;
    CHECK_EQ(gui.Probe.EditableEntities().size(), size_t(2));
    CHECK(AppStub::WasPrinted("PALETTE"));
    CHECK(AppStub::WasPrinted("INSPECTOR"));
    CHECK(AppStub::WasPrinted("Playable"));
    CHECK(AppStub::WasPrinted("1 Soldier"));
    CHECK(AppStub::WasPrinted("7 Wall"));
    CHECK(AppStub::Get().LinesDrawn > 0);
}

TEST_CASE("Editor GUI: clicking the ground places the prefab under the mouse")
{
    GuiHarness gui;
    REQUIRE(gui.ClickLabel("1 Soldier"));
    CHECK(AppStub::WasPrinted("> 1 Soldier"));

    // Several places across the viewport, including far from the centre
    const Vec3 targets[] = {{6, 0, -4}, {-8, 0, 10}, {12, 0, 15}, {-10, 0, -9}};
    for (const Vec3& target : targets)
    {
        gui.ClickWorld(target);
        std::vector<Entity> soldiers = gui.ObjectsOfKind(EntityKind::Soldier);
        REQUIRE(!soldiers.empty());
        Vec3 placed = gui.Probe.GetPosition(soldiers.back());
        // Snap step is 0.5, so the placement is within half a grid cell
        CHECK(DistanceXZ(placed, target) <= 0.36f);
    }
    CHECK_EQ(gui.ObjectsOfKind(EntityKind::Soldier).size(), size_t(4));
}

TEST_CASE("Editor GUI: clicks on the panels never reach the viewport")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_1); // place soldiers
    size_t before = gui.Probe.EditableEntities().size();
    gui.Click(80.0f, 300.0f);                                    // palette
    gui.Click(APP_VIRTUAL_WIDTH - 100.0f, 300.0f);               // inspector
    gui.Click(APP_VIRTUAL_WIDTH * 0.5f, APP_VIRTUAL_HEIGHT - 5); // toolbar
    gui.Click(APP_VIRTUAL_WIDTH * 0.5f, 20.0f);                  // status bar
    CHECK_EQ(gui.Probe.EditableEntities().size(), before);
}

TEST_CASE("Editor GUI: select, drag, rotate, delete and undo with the mouse and keys")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_3); // player tank
    gui.ClickWorld({4, 0, -6});
    REQUIRE(gui.ObjectsOfKind(EntityKind::PlayerTank).size() == 1);
    Entity tank = gui.ObjectsOfKind(EntityKind::PlayerTank)[0];

    gui.PressKey(App::KEY_SPACE); // select tool
    gui.ClickWorld({4, 0, -6});
    CHECK(AppStub::WasPrinted("Player Tank  #" + std::to_string(tank)));

    // Drag records exactly one undo step
    gui.DragWorld({4, 0, -6}, {-7, 0, 9});
    Vec3 moved = gui.Probe.GetPosition(tank);
    CHECK(DistanceXZ(moved, {-7, 0, 9}) <= 0.36f);
    CHECK(AppStub::WasPrinted("Undo 2")); // place + drag

    gui.PressKey(App::KEY_R);
    CHECK(std::fabs(gui.Probe.GetYaw(tank) - 45.0f) < 0.1f);

    gui.PressKey(App::KEY_X);
    CHECK(!ECS.IsEntityAlive(tank));

    gui.PressKey(App::KEY_U); // undo delete
    REQUIRE(ECS.IsEntityAlive(tank));
    gui.PressKey(App::KEY_U); // undo rotation
    CHECK(std::fabs(gui.Probe.GetYaw(tank)) < 0.1f);
    gui.PressKey(App::KEY_U); // undo drag
    CHECK(DistanceXZ(gui.Probe.GetPosition(tank), {4, 0, -6}) <= 0.36f);
    gui.PressKey(App::KEY_Y); // redo drag
    CHECK(DistanceXZ(gui.Probe.GetPosition(tank), {-7, 0, 9}) <= 0.36f);
}

TEST_CASE("Editor GUI: clicking an object without moving it does not create an undo step")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_6); // crystal
    gui.ClickWorld({-5, 0, 5});
    gui.PressKey(App::KEY_SPACE);
    gui.ClickWorld({-5, 0, 5});
    CHECK(AppStub::WasPrinted("Crystal  #"));
    CHECK(AppStub::WasPrinted("Undo 1 "));
}

TEST_CASE("Editor GUI: the base and selector can not be deleted")
{
    GuiHarness gui;
    gui.ClickWorld({0, 0, 0});
    CHECK(AppStub::WasPrinted("Player Base  #"));
    gui.PressKey(App::KEY_X);
    CHECK_EQ(gui.ObjectsOfKind(EntityKind::PlayerBase).size(), size_t(1));
    gui.Frame();
    CHECK(AppStub::WasPrinted("can not be deleted"));
}

TEST_CASE("Editor GUI: inspector and brush steppers edit values")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_1);
    // Brush health 100 -> 120 before placing
    REQUIRE(gui.ClickStepper("HP 100", +1));
    REQUIRE(gui.ClickStepper("HP 110", +1));
    gui.ClickWorld({3, 0, 3});
    Entity soldier = gui.ObjectsOfKind(EntityKind::Soldier).at(0);
    CHECK_EQ(gui.Probe.GetHealth(soldier), 120);

    // The placed soldier is selected, edit it in the inspector
    REQUIRE(gui.ClickStepper("Health 120", -1));
    CHECK_EQ(gui.Probe.GetHealth(soldier), 110);
    REQUIRE(gui.ClickStepper("Battalion 1", +1));
    CHECK_EQ(gui.Probe.GetBattalion(soldier), 2);

    // Scene settings
    REQUIRE(gui.ClickStepper("Crystals 25", +1));
    CHECK_EQ(gui.Probe.GetStartingCrystals(), 30);
    REQUIRE(gui.ClickStepper("Round 1", +1));
    CHECK_EQ(gui.Probe.GetRoundNumber(), 2);
}

TEST_CASE("Editor GUI: toolbar save, new, load and undo")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_5); // enemy tank
    gui.ClickWorld({10, 0, 10});
    gui.ClickWorld({-10, 0, 12});
    REQUIRE(gui.ObjectsOfKind(EntityKind::EnemyTank).size() == 2);
    CHECK(AppStub::WasPrinted("my_scene_1 *")); // unsaved changes marker

    REQUIRE(gui.ClickLabel("Save"));
    CHECK(fs::exists("data/scenes/my_scene_1.ubsave"));
    CHECK(AppStub::WasPrinted("Saved data/scenes/my_scene_1.ubsave"));
    CHECK(!AppStub::WasPrinted("my_scene_1 *"));

    REQUIRE(gui.ClickLabel("New"));
    CHECK(gui.ObjectsOfKind(EntityKind::EnemyTank).empty());

    REQUIRE(gui.ClickLabel("Load"));
    CHECK_EQ(gui.ObjectsOfKind(EntityKind::EnemyTank).size(), size_t(2));

    gui.PressKey(App::KEY_6);
    gui.ClickWorld({0, 0, 12});
    CHECK_EQ(gui.ObjectsOfKind(EntityKind::Crystal).size(), size_t(1));
    REQUIRE(gui.ClickLabel("Undo"));
    CHECK(gui.ObjectsOfKind(EntityKind::Crystal).empty());
    REQUIRE(gui.ClickLabel("Redo"));
    CHECK_EQ(gui.ObjectsOfKind(EntityKind::Crystal).size(), size_t(1));

    // The saved file is a valid scene for the headless editor as well
    Editor::SceneEditor reader;
    CHECK(reader.LoadScene("data/scenes/my_scene_1.ubsave").Success);
}

TEST_CASE("Editor GUI: the scene list picks the slot and does not click through")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_1); // place tool, so a stray click on "Select" would show
    REQUIRE(gui.ClickLabel("my_scene_1"));
    // The open list shows every slot below the toolbar
    const AppStub::State::PrintedText* third = nullptr;
    for (const auto& printed : AppStub::Get().Printed)
    {
        if (printed.Text == "my_scene_3")
            third = &printed;
    }
    REQUIRE(third != nullptr);
    // The third entry lies on top of the palette's "Select" button
    gui.Click(third->X + 3.0f, third->Y + 3.0f);
    CHECK(AppStub::WasPrinted("my_scene_3"));
    CHECK(AppStub::WasPrinted("> 1 Soldier")); // tool unchanged

    REQUIRE(gui.ClickLabel("Save"));
    CHECK(fs::exists("data/scenes/my_scene_3.ubsave"));
}

TEST_CASE("Editor GUI: play saves a play test copy and loads it into the main level")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_2);
    gui.ClickWorld({-4, 0, -4});
    REQUIRE(gui.ClickLabel("Play"));
    REQUIRE(g_RequestedLoads.size() == 1);
    CHECK_EQ(g_RequestedLoads[0], std::string(SceneEditorScene::PLAYTEST_PATH));
    REQUIRE(g_PlaytestReturnScenes.size() == 1);
    CHECK_EQ(g_PlaytestReturnScenes[0], std::string("SceneEditor"));
    REQUIRE(fs::exists(SceneEditorScene::PLAYTEST_PATH));

    Serialization::WorldSnapshot snapshot;
    std::vector<std::uint8_t> bytes;
    std::string error;
    REQUIRE(Serialization::WorldSerializer::ReadFile(
            SceneEditorScene::PLAYTEST_PATH, bytes, error));
    Serialization::LoadResult parsed =
            Serialization::WorldSerializer(Fixture::Registry()).Parse(bytes, snapshot);
    REQUIRE(parsed.Success);
    CHECK_EQ(parsed.Metadata["Scene"], std::string("MainLevel"));
}

TEST_CASE("Editor GUI: menu returns to the title screen")
{
    GuiHarness gui;
    REQUIRE(gui.ClickLabel("Menu"));
    REQUIRE(g_SceneSwitches.size() == 1);
    CHECK_EQ(g_SceneSwitches[0], std::string("TitleScreen"));
}

TEST_CASE("Editor GUI: camera pans and zooms with the keyboard")
{
    GuiHarness gui;
    Vec2 before = gui.ScreenOf({0, 0, 0});
    gui.HoldKey(App::KEY_D, 20);
    Vec2 panned = gui.ScreenOf({0, 0, 0});
    CHECK(std::fabs(panned.X - before.X) > 20.0f);

    Vec2 a = gui.ScreenOf({-5, 0, 0});
    Vec2 b = gui.ScreenOf({5, 0, 0});
    float widthBefore = std::fabs(b.X - a.X);
    gui.HoldKey(App::KEY_Z, 20); // zoom in
    a = gui.ScreenOf({-5, 0, 0});
    b = gui.ScreenOf({5, 0, 0});
    CHECK(std::fabs(b.X - a.X) > widthBefore);

    // Placement is still accurate after moving the camera
    gui.PressKey(App::KEY_7);
    Vec3 target(-6, 0, 3);
    gui.ClickWorld(target);
    std::vector<Entity> walls = gui.ObjectsOfKind(EntityKind::Wall);
    REQUIRE(walls.size() == 1);
    CHECK(DistanceXZ(gui.Probe.GetPosition(walls[0]), target) <= 0.36f);
}

TEST_CASE("Editor GUI: the editor keeps the world frozen")
{
    GuiHarness gui;
    SceneEditorScene scene;
    CHECK(!scene.SimulatesWorld());
}

TEST_CASE("Editor GUI: placement snaps to the grid unless snapping is turned off")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_6); // crystal
    gui.ClickWorld({6.2f, 0, -3.85f});
    Vec3 snapped = gui.Probe.GetPosition(gui.ObjectsOfKind(EntityKind::Crystal).at(0));
    CHECK(Fixture::Same(snapped.X, 6.0f));
    CHECK(Fixture::Same(snapped.Z, -4.0f));

    gui.PressKey(App::KEY_G); // snapping off
    gui.ClickWorld({-7.2f, 0, 8.3f});
    Vec3 free = gui.Probe.GetPosition(gui.ObjectsOfKind(EntityKind::Crystal).at(1));
    CHECK(DistanceXZ(free, {-7.2f, 0, 8.3f}) < 0.15f);
    CHECK(std::fabs(free.X - std::round(free.X * 2.0f) / 2.0f) > 0.01f); // off grid
}

TEST_CASE("Editor GUI: replacing the world drops the per entity render caches")
{
    GuiHarness gui;
    auto vertices = ECS.GetResource<VertexBuffer>();
    auto indices = ECS.GetResource<IndexBuffer>();
    auto constants = ECS.GetResource<RenderConstants>();
    auto pretendRendered = [&] {
        // What the MeshHandler / ShaderHandler hold after drawing entity 5
        vertices->Buffer.resize(12);
        indices->Buffer.resize(6);
        constants->EntityToVertexRange[5] = {0, 12};
        constants->EntityToFragShaderType[5] = BlinnPhongID;
    };
    auto cachesEmpty = [&] {
        return vertices->Buffer.empty() && indices->Buffer.empty() &&
               constants->EntityToVertexRange.empty() && constants->EntityToFragShaderType.empty();
    };

    gui.PressKey(App::KEY_1);
    gui.ClickWorld({2, 0, 2});
    pretendRendered();
    // A normal edit keeps the caches (the render systems update them)
    gui.ClickWorld({3, 0, 2});
    CHECK(!cachesEmpty());

    REQUIRE(gui.ClickLabel("Undo"));
    CHECK(cachesEmpty());

    pretendRendered();
    REQUIRE(gui.ClickLabel("Redo"));
    CHECK(cachesEmpty());

    pretendRendered();
    REQUIRE(gui.ClickLabel("New"));
    CHECK(cachesEmpty());
}

TEST_CASE("Editor GUI: coming back from a play test reopens the played scene")
{
    GuiHarness gui;
    gui.PressKey(App::KEY_4); // enemy
    gui.ClickWorld({9, 0, 9});
    gui.ClickWorld({11, 0, 9});
    REQUIRE(gui.ObjectsOfKind(EntityKind::EnemySoldier).size() == 2);
    REQUIRE(gui.ClickLabel("Play"));

    // The main level ran and TAB brought us back: Setup runs again
    gui.ReenterScene();
    CHECK_EQ(gui.ObjectsOfKind(EntityKind::EnemySoldier).size(), size_t(2));
    CHECK(AppStub::WasPrinted("Back from the play test"));

    // Entering the editor any other way starts a new scene
    gui.ReenterScene();
    CHECK(gui.ObjectsOfKind(EntityKind::EnemySoldier).empty());
}
