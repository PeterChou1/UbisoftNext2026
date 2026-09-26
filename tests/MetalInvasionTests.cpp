//---------------------------------------------------------------------------------
// MetalInvasionTests.cpp
//---------------------------------------------------------------------------------
//
// Metal Invasion rebuilt on scenes + scripts, played through real frames:
// data/scenes/metal_invasion.ubsave is loaded like the Game loads it and the
// tests drive it through the MetalInvasion scene script's commands, the
// mouse and the keyboard
//
#include "AppStub.h"
#include "Assets.h"
#include "Camera.h"
#include "FragShaderTag.h"
#include "Map.h"
#include "Scripting/ScriptSystem.h"
#include "Scripts/MetalInvasion/MIGame.h"
#include "Scripts/MetalInvasion/MINames.h"
#include "Scripts/MetalInvasion/MIUnits.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using Phase = MetalInvasion::Phase;
using Mode = MetalInvasion::Mode;
using Item = MetalInvasion::Item;

namespace
{
    constexpr float FRAME_MS = 20.0f;

    void Play(float seconds)
    {
        TestEnvironment::RunFrames(static_cast<int>(std::lround(seconds * 1000.0f / FRAME_MS)), FRAME_MS);
    }

    MetalInvasion* Game() { return dynamic_cast<MetalInvasion*>(GameSceneManager.Scripts().GetSceneScript()); }

    template <typename T>
    T* ScriptOn(Entity e)
    {
        return dynamic_cast<T*>(GameSceneManager.Scripts().GetScript(e));
    }

    /**
     * \brief Load the scene like the Game does, optionally overriding scene
     *        script parameters, and run until the game has started
     */
    MetalInvasion& StartGame(const std::map<std::string, float>& params = {{"PrepTime", 1000.0f}})
    {
        AppStub::Reset();
        std::string error;
        REQUIRE(GameSceneManager.LoadGame(GameManager::ScenePath("metal_invasion"), error));
        for (const auto& kv : params)
            ECS.GetResource<SceneSettings>()->SceneParams[kv.first] = kv.second;
        TestEnvironment::RunFrames(2, FRAME_MS);
        REQUIRE(Game() != nullptr);
        return *Game();
    }

    std::vector<Entity> Tagged(const char* tag) { return SceneObjects::FindByTag(tag); }

    float Distance(const Vec3& a, const Vec3& b)
    {
        return std::sqrt((a.X - b.X) * (a.X - b.X) + (a.Z - b.Z) * (a.Z - b.Z));
    }

    void Kill(Entity e)
    {
        if (auto* unit = ScriptOn<MIUnit>(e))
            unit->Damage(1e6f);
    }

    Vec2 ScreenOf(const Vec3& world) { return ECS.GetResource<Camera>()->WorldPointToScreenSpace(world); }

    void ClickScreen(float x, float y, bool right = false)
    {
        AppStub::Get().MouseX = x;
        AppStub::Get().MouseY = y;
        (right ? AppStub::Get().RightDown : AppStub::Get().LeftDown) = true;
        TestEnvironment::RunFrame(FRAME_MS);
        (right ? AppStub::Get().RightDown : AppStub::Get().LeftDown) = false;
        TestEnvironment::RunFrame(FRAME_MS);
    }

    void ClickGround(const Vec3& world, bool right = false)
    {
        Vec2 s = ScreenOf(world);
        ClickScreen(s.X, s.Y, right);
    }
} // namespace

TEST_CASE("Metal Invasion: the scene starts in preparation with crystals around the base")
{
    MetalInvasion& game = StartGame();
    CHECK(game.GetPhase() == Phase::Preparation);
    CHECK_EQ(game.Round(), 1);
    CHECK_EQ(game.Crystals(), 25);
    CHECK(GameSceneManager.Scripts().MissingScripts().empty());

    REQUIRE(game.BaseEntity() != NULL_ENTITY);
    CHECK_EQ(game.BaseHealth(), 1000.0f);
    // The base blocks the path finding grid
    CHECK(ECS.HasComponent<AIObstacle>(game.BaseEntity()));

    std::vector<Entity> crystals = Tagged(MI::Tags::Crystal);
    CHECK_EQ(crystals.size(), size_t(10));
    for (Entity c : crystals)
    {
        float d = Distance(SceneObjects::GetPosition(c), {0, 0, 0});
        CHECK(d >= 9.99f);
        CHECK(d <= 20.01f);
        CHECK(ECS.HasComponent<AIObstacle>(c));
        REQUIRE(ScriptOn<MICrystal>(c) != nullptr);
        CHECK_EQ(ScriptOn<MICrystal>(c)->Amount(), 30);
    }
    CHECK(AppStub::WasPrinted("Crystal Count: 25"));
    CHECK(AppStub::WasPrinted("Preparation Phase"));
    CHECK(AppStub::WasPrinted("Round: 1"));
}

TEST_CASE("Metal Invasion: the shop spends crystals and spawns battalions at the base")
{
    MetalInvasion& game = StartGame();
    CHECK(game.Purchase(Item::Soldiers));
    CHECK_EQ(game.Crystals(), 15);
    CHECK(!game.Purchase(Item::Tank)); // 50 crystals
    CHECK(game.Purchase(Item::Support));
    CHECK_EQ(game.Crystals(), 0);
    CHECK(!game.Purchase(Item::Soldiers));
    TestEnvironment::RunFrame(FRAME_MS);

    std::vector<Entity> units = Tagged(MI::Tags::Unit);
    CHECK_EQ(units.size(), size_t(10));
    int soldiers = 0, support = 0;
    for (Entity u : units)
    {
        if (ScriptOn<MISoldier>(u))
            ++soldiers;
        if (ScriptOn<MISupport>(u))
            ++support;
        CHECK(Distance(SceneObjects::GetPosition(u), {0, 0, -3.5f}) < 1.5f);
        CHECK(SceneObjects::GetBodyType(u) == SceneObjects::BodyType::Dynamic);
    }
    CHECK_EQ(soldiers, 5);
    CHECK_EQ(support, 5);
    // Each purchase is its own battalion
    CHECK(ScriptOn<MIPlayerUnit>(units.front())->Battalion() != ScriptOn<MIPlayerUnit>(units.back())->Battalion());
}

TEST_CASE("Metal Invasion: select a battalion, order it to move, deselect")
{
    MetalInvasion& game = StartGame();
    game.Purchase(Item::Soldiers);
    game.Purchase(Item::Soldiers);
    TestEnvironment::RunFrame(FRAME_MS);
    std::vector<Entity> units = Tagged(MI::Tags::Unit);
    REQUIRE(units.size() == 10);

    game.Select(units.front(), false);
    std::vector<Entity> selected = game.SelectedUnits();
    CHECK_EQ(selected.size(), size_t(5));
    TestEnvironment::RunFrame(FRAME_MS);
    // Selected units are drawn red
    CHECK(ECS.GetComponent<FragShaderTag>(units.front()).FragAssetId == RedShaderID);
    CHECK(ECS.GetComponent<FragShaderTag>(units.back()).FragAssetId == BlinnPhongID);

    Vec3 target(8, 0, -6);
    game.OrderMove(target);
    Play(4.0f);
    for (Entity u : selected)
        CHECK(Distance(SceneObjects::GetPosition(u), target) < 2.5f);
    for (Entity u : units)
    {
        if (std::find(selected.begin(), selected.end(), u) == selected.end())
            CHECK(Distance(SceneObjects::GetPosition(u), {0, 0, -3.5f}) < 2.0f);
    }

    // Space merges: selecting the other battalion adds it to this one
    game.Select(units.back(), true);
    CHECK_EQ(game.SelectedUnits().size(), size_t(10));
    int battalion = ScriptOn<MIPlayerUnit>(units.back())->Battalion();
    for (Entity u : units)
        CHECK_EQ(ScriptOn<MIPlayerUnit>(u)->Battalion(), battalion);

    game.ClearSelection();
    CHECK(game.SelectedUnits().empty());
    CHECK(!game.HasMoveOrder());
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(ECS.GetComponent<FragShaderTag>(units.front()).FragAssetId == BlinnPhongID);
}

TEST_CASE("Metal Invasion: units find their way around walls")
{
    MetalInvasion& game = StartGame({{"PrepTime", 1000.0f}, {"CrystalCount", 0.0f}});
    // A wall standing between the unit and its destination
    game.AddCrystals(100);
    REQUIRE(game.Purchase(Item::Wall));
    game.MoveWallPreview({4, 0, 8});
    REQUIRE(game.PlaceWall());
    Entity wall = Tagged(MI::Tags::Wall).front();

    Entity unit = MI::SpawnSoldier({0, 0, 8}, 99);
    TestEnvironment::RunFrame(FRAME_MS);
    game.Select(unit, false);
    Vec3 target(8, 0, 8);
    game.OrderMove(target);
    bool touchedWall = false;
    for (int i = 0; i < 400 && Distance(SceneObjects::GetPosition(unit), target) > 1.2f; ++i)
    {
        TestEnvironment::RunFrame(FRAME_MS);
        touchedWall = touchedWall || SceneObjects::Contains(wall, SceneObjects::GetPosition(unit), 0.1f);
    }
    CHECK(Distance(SceneObjects::GetPosition(unit), target) <= 1.2f);
    CHECK(!touchedWall);
}

TEST_CASE("Metal Invasion: support units mine crystals")
{
    MetalInvasion& game = StartGame({{"PrepTime", 1000.0f}, {"CrystalCount", 0.0f}});
    Entity crystal = MI::SpawnCrystal({3, 0, 3}, 30);
    MI::SpawnBattalion({3, 0, 1.8f}, 4, 50, true);
    // Units in range mine at once, then every 2 seconds (like the original)
    int before = game.Crystals();
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(game.Crystals(), before + 4);
    Play(2.1f);
    CHECK_EQ(game.Crystals(), before + 8);
    CHECK_EQ(ScriptOn<MICrystal>(crystal)->Amount(), 22);

    // An exhausted deposit disappears
    ScriptOn<MICrystal>(crystal)->Mine(100);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(!ECS.IsEntityAlive(crystal));
}

TEST_CASE("Metal Invasion: soldiers and enemies shoot each other in range")
{
    StartGame({{"PrepTime", 1000.0f}, {"CrystalCount", 0.0f}});
    Entity soldier = MI::SpawnSoldier({10, 0, 10}, 7);
    Entity enemy = MI::SpawnEnemySoldier({10, 0, 13}, 1.0f, 30.0f);
    Entity far = MI::SpawnEnemySoldier({-10, 0, 20}, 0.0f, 30.0f);
    TestEnvironment::RunFrames(2, FRAME_MS);
    // Both fired at once: a laser each, the enemy took 20
    CHECK(SceneObjects::FindByName("Laser") == NULL_ENTITY); // names are numbered
    int lasers = 0;
    for (Entity e : ECS.Visit<SceneObject>())
        lasers += ECS.GetComponent<SceneObject>(e).Name.rfind("Laser", 0) == 0 ? 1 : 0;
    CHECK(lasers >= 2);
    CHECK_EQ(ScriptOn<MIUnit>(enemy)->Health(), 10.0f);
    CHECK_EQ(ScriptOn<MIUnit>(soldier)->Health(), 90.0f);
    CHECK_EQ(ScriptOn<MIUnit>(far)->Health(), 30.0f);
    // Second volley 2 s later kills it
    Play(2.1f);
    CHECK(!ECS.IsEntityAlive(enemy));
    CHECK(ECS.IsEntityAlive(soldier));
    // Lasers are short lived effects
    Play(0.5f);
    lasers = 0;
    for (Entity e : ECS.Visit<SceneObject>())
        lasers += ECS.GetComponent<SceneObject>(e).Name.rfind("Laser", 0) == 0 ? 1 : 0;
    CHECK_EQ(lasers, 0);
}

TEST_CASE("Metal Invasion: enemies march on the base and damage it")
{
    MetalInvasion& game = StartGame({{"PrepTime", 1000.0f}, {"CrystalCount", 0.0f}});
    Entity enemy = MI::SpawnEnemySoldier({0, 0, 14}, 5.0f, 100.0f);
    Play(4.0f);
    // Stopped within firing range of the base and shooting it
    float d = Distance(SceneObjects::GetPosition(enemy), {0, 0, 0});
    CHECK(d <= 5.01f);
    CHECK(d > 2.0f);
    CHECK(game.BaseHealth() < 1000.0f);
    CHECK(std::fmod(1000.0f - game.BaseHealth(), 10.0f) == 0.0f);
}

TEST_CASE("Metal Invasion: tanks turn their turret and fire explosive bullets")
{
    StartGame({{"PrepTime", 1000.0f}, {"CrystalCount", 0.0f}});
    Entity tank = MI::SpawnTank({-10, 0, 10}, 3);
    // Enemies stand still (speed 0) behind the tank, in range
    Entity e1 = MI::SpawnEnemySoldier({-10, 0, 17}, 0.0f, 50.0f);
    TestEnvironment::RunFrame(FRAME_MS);
    Entity cannon = SceneObjects::FindByName(ECS.GetComponent<SceneObject>(tank).Name + " cannon");
    REQUIRE(cannon != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<SceneObject>(cannon).Tag, std::string(MI::Tags::Turret));

    bool sawBullet = false;
    for (int i = 0; i < 150 && ECS.IsEntityAlive(e1); ++i)
    {
        TestEnvironment::RunFrame(FRAME_MS);
        for (Entity e : ECS.Visit<SceneObject>())
            sawBullet = sawBullet || ECS.GetComponent<SceneObject>(e).Name.rfind("Bullet", 0) == 0;
    }
    CHECK(sawBullet);
    CHECK(!ECS.IsEntityAlive(e1));
    // The turret faces the target (+Z), it sits on the hull
    CHECK(std::fabs(SceneObjects::GetYaw(cannon)) < 10.0f || SceneObjects::GetYaw(cannon) > 350.0f);
    CHECK(Distance(SceneObjects::GetPosition(cannon), SceneObjects::GetPosition(tank)) < 0.01f);

    // A destroyed tank takes its turret with it
    Kill(tank);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(!ECS.IsEntityAlive(tank));
    CHECK(!ECS.IsEntityAlive(cannon));
}

TEST_CASE("Metal Invasion: walls are previewed, validated, built, and can be destroyed")
{
    MetalInvasion& game = StartGame({{"PrepTime", 1000.0f}, {"CrystalCount", 0.0f}});
    REQUIRE(game.Purchase(Item::Wall));
    CHECK(game.GetMode() == Mode::PlaceWall);
    CHECK_EQ(game.Crystals(), 15);
    REQUIRE(game.WallPreview() != NULL_ENTITY);
    CHECK(!game.Purchase(Item::Soldiers)); // one thing at a time

    // Not on the base
    game.MoveWallPreview({0, 0, 1});
    CHECK(!game.WallPreviewValid());
    CHECK(!game.PlaceWall());
    CHECK(ECS.GetComponent<Shape2D>(game.WallPreview()).Color.X > 0.8f); // shown red

    // Right click cancels and refunds
    game.CancelWall();
    CHECK_EQ(game.Crystals(), 25);
    CHECK(game.GetMode() == Mode::Command);

    REQUIRE(game.Purchase(Item::Wall));
    game.MoveWallPreview({8, 0, -8});
    game.RotateWallPreview();
    CHECK(game.WallPreviewValid());
    Entity wall = game.WallPreview();
    REQUIRE(game.PlaceWall());
    CHECK(game.GetMode() == Mode::Command);
    CHECK_EQ(ECS.GetComponent<SceneObject>(wall).Tag, std::string(MI::Tags::Wall));
    CHECK(SceneObjects::GetBodyType(wall) == SceneObjects::BodyType::Static);
    CHECK_EQ(SceneObjects::GetYaw(wall), 90.0f);
    REQUIRE(ECS.HasComponent<AIObstacle>(wall));
    // Rotated: long side along X
    CHECK(ECS.GetComponent<AIObstacle>(wall).Width > ECS.GetComponent<AIObstacle>(wall).Height);
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ScriptOn<MIWall>(wall) != nullptr);
    CHECK_EQ(ScriptOn<MIWall>(wall)->Health(), 250.0f);

    Kill(wall);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(!ECS.IsEntityAlive(wall));
}

TEST_CASE("Metal Invasion: rounds spawn waves, get harder, and the next round starts")
{
    MetalInvasion& game = StartGame({{"PrepTime", 0.2f}, {"InvasionTime", 0.5f}, {"SpawnInterval", 10.0f}, {"CrystalCount", 2.0f}});
    CHECK(game.GetPhase() == Phase::Preparation);
    CHECK_EQ(Tagged(MI::Tags::Crystal).size(), size_t(2));
    Play(0.3f);
    CHECK(game.GetPhase() == Phase::Invasion);
    TestEnvironment::RunFrame(FRAME_MS);
    std::vector<Entity> enemies = Tagged(MI::Tags::Enemy);
    // One wave of SpawnVolume 1: a battalion of 5 or a tank
    CHECK((enemies.size() == 5 || enemies.size() == 1));
    for (Entity e : enemies)
        CHECK(std::fabs(Distance(SceneObjects::GetPosition(e), {0, 0, 0}) - 24.0f) < 1.5f);

    // The round lasts until every enemy is dead
    Play(1.0f);
    CHECK(game.GetPhase() == Phase::Invasion);
    CHECK(AppStub::WasPrinted("Kill All Enemy"));
    for (Entity e : Tagged(MI::Tags::Enemy))
        Kill(e);
    Play(0.2f);
    CHECK_EQ(game.Round(), 2);
    CHECK_EQ(game.SpawnVolume(), 3);
    CHECK(game.GetPhase() == Phase::Preparation);
    // Enter starts the invasion without waiting
    AppStub::Get().Keys[App::KEY_ENTER] = true;
    TestEnvironment::RunFrame(FRAME_MS);
    AppStub::Get().Keys[App::KEY_ENTER] = false;
    TestEnvironment::RunFrames(2, FRAME_MS);
    CHECK(game.GetPhase() == Phase::Invasion);
}

TEST_CASE("Metal Invasion: losing the base ends the game, Enter plays again")
{
    MetalInvasion& game = StartGame();
    game.Purchase(Item::Soldiers);
    TestEnvironment::RunFrame(FRAME_MS);
    Kill(game.BaseEntity());
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(game.GetPhase() == Phase::GameOver);
    CHECK(AppStub::WasPrinted("You Lose"));
    CHECK(!game.Purchase(Item::Support));

    AppStub::Get().Keys[App::KEY_ENTER] = true;
    TestEnvironment::RunFrame(FRAME_MS);
    AppStub::Get().Keys[App::KEY_ENTER] = false;
    TestEnvironment::RunFrames(3, FRAME_MS);
    // The scene file was reloaded: a new game
    REQUIRE(Game() != nullptr);
    CHECK(Game()->GetPhase() == Phase::Preparation);
    CHECK_EQ(Game()->Crystals(), 25);
    CHECK(Tagged(MI::Tags::Unit).empty());
    CHECK_EQ(Game()->BaseHealth(), 1000.0f);
}

TEST_CASE("Metal Invasion: mouse - click the base, buy from the shop, select and order units")
{
    MetalInvasion& game = StartGame();
    Vec2 base = ScreenOf({0, 1.0f, 0});
    ClickScreen(base.X, base.Y);
    CHECK(game.GetMode() == Mode::BaseMenu);
    // The click that opened the shop bought nothing
    CHECK_EQ(game.Crystals(), 25);
    const auto* button = AppStub::FindPrinted("Purchase Soldiers");
    REQUIRE(button != nullptr);
    ClickScreen(button->X + 80.0f, button->Y + 5.0f);
    CHECK_EQ(game.Crystals(), 15);
    CHECK(game.GetMode() == Mode::Command);
    TestEnvironment::RunFrame(FRAME_MS);
    std::vector<Entity> units = Tagged(MI::Tags::Unit);
    REQUIRE(units.size() == 5);

    // Click a unit: its battalion is selected
    ClickGround(SceneObjects::GetPosition(units[2]));
    CHECK_EQ(game.SelectedUnits().size(), size_t(5));
    // Click the ground: move order
    ClickGround({6, 0, -6});
    CHECK(game.HasMoveOrder());
    Play(3.0f);
    CHECK(Distance(SceneObjects::GetPosition(units[0]), {6, 0, -6}) < 2.5f);
    // Right click deselects
    ClickGround({0, 0, -8}, true);
    CHECK(game.SelectedUnits().empty());

    // The shop's Back button and right click close it
    ClickScreen(base.X, base.Y);
    CHECK(game.GetMode() == Mode::BaseMenu);
    ClickScreen(10.0f, 10.0f, true);
    CHECK(game.GetMode() == Mode::Command);

    // WASD pans the camera
    Vec2 before = ScreenOf({0, 0, 0});
    AppStub::Get().Keys[App::KEY_D] = true;
    TestEnvironment::RunFrames(10, FRAME_MS);
    AppStub::Get().Keys[App::KEY_D] = false;
    CHECK(std::fabs(ScreenOf({0, 0, 0}).X - before.X) > 20.0f);
}

TEST_CASE("Metal Invasion: a game saved mid-round loads with every unit and its behaviour")
{
    MetalInvasion& game = StartGame();
    game.AddCrystals(100);
    game.Purchase(Item::Soldiers);
    game.Purchase(Item::Tank);
    TestEnvironment::RunFrames(2, FRAME_MS);
    size_t units = Tagged(MI::Tags::Unit).size();
    size_t turrets = Tagged(MI::Tags::Turret).size();
    CHECK_EQ(turrets, size_t(1));

    std::string path = (std::filesystem::temp_directory_path() / "ubisoft_next_mi_save.ubsave").string();
    std::string error;
    REQUIRE(GameSceneManager.SaveGame(path, error));
    REQUIRE(GameSceneManager.LoadGame(path, error));
    TestEnvironment::RunFrames(2, FRAME_MS);
    CHECK(GameSceneManager.Scripts().MissingScripts().empty());
    CHECK_EQ(Tagged(MI::Tags::Unit).size(), units);
    // The tank found its saved cannon instead of making a second one
    CHECK_EQ(Tagged(MI::Tags::Turret).size(), turrets);
    for (Entity u : Tagged(MI::Tags::Unit))
        CHECK(ScriptOn<MIPlayerUnit>(u) != nullptr);
    std::filesystem::remove(path);
}
