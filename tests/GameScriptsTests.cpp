//---------------------------------------------------------------------------------
// GameScriptsTests.cpp
//---------------------------------------------------------------------------------
//
// The game's C++ scripts (src/Game/Scripts) running on scenes built exactly
// like the editor builds them, through real GameManager frames
//
#include "AppStub.h"
#include "Scripting/ScriptSystem.h"
#include "Scripts/CollectGame.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>

using SceneObjects::BodyType;

namespace
{
    constexpr float FRAME_MS = 20.0f;

    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    // Seconds of game time
    void Play(float seconds)
    {
        int frames = static_cast<int>(std::lround(seconds * 1000.0f / FRAME_MS));
        TestEnvironment::RunFrames(frames, FRAME_MS);
    }

    Entity Scripted(const std::string& name,
                    Shape2DType type,
                    const Vec3& position,
                    const std::string& script,
                    const std::map<std::string, float>& params = {},
                    BodyType body = BodyType::None,
                    const std::string& tag = "")
    {
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf(name, type, position, body);
        desc.Script = script;
        desc.ScriptParams = params;
        desc.Tag = tag;
        return SceneObjects::CreateShape(desc);
    }

    Entity Player(const Vec3& position, BodyType body = BodyType::Dynamic)
    {
        return Scripted("Player", Shape2DType::Circle, position, "PlayerController", {{"Speed", 5.0f}}, body, "Player");
    }

    CollectGame* Game() { return dynamic_cast<CollectGame*>(GameSceneManager.Scripts().GetSceneScript()); }

    void UseCollectGame(float level, float lives)
    {
        auto settings = ECS.GetResource<SceneSettings>();
        settings->SceneScript = "CollectGame";
        settings->SceneParams = {{"Level", level}, {"Lives", lives}};
    }

    // Save the current world as a scene file and start it like the Game does
    std::string SaveAndPlay(const std::string& name)
    {
        std::string path = (std::filesystem::temp_directory_path() / ("ubisoft_next_game_" + name + ".ubsave")).string();
        std::string error;
        REQUIRE(GameSceneManager.SaveGame(path, error));
        REQUIRE(GameSceneManager.LoadGame(path, error));
        return path;
    }

    void Setup()
    {
        Fixture::FreshWorld();
        AppStub::Reset();
    }
} // namespace

TEST_CASE("Game scripts: Rotator spins at its speed")
{
    Setup();
    Entity e = Scripted("Spinner", Shape2DType::Polygon, {0, 0, 0}, "Rotator", {{"Speed", 90.0f}});
    Play(0.5f);
    CHECK(Near(SceneObjects::GetYaw(e), 45.0f, 0.05f));
}

TEST_CASE("Game scripts: Patrol goes back and forth along its facing")
{
    Setup();
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Guard", Shape2DType::Rectangle, {2, 0, 3});
    desc.YawDegrees = 90.0f; // facing +X
    desc.Script = "Patrol";
    desc.ScriptParams = {{"Distance", 2.0f}, {"Speed", 4.0f}};
    Entity e = SceneObjects::CreateShape(desc);

    TestEnvironment::RunFrame(FRAME_MS);
    // Starts where it was placed, moving forward
    CHECK(Near(SceneObjects::GetPosition(e).X, 2.0f + 4.0f * FRAME_MS / 1000.0f));
    TestEnvironment::RunFrames(11, FRAME_MS);
    // 12 frames x 0.02 s x 4 units / s
    CHECK(Near(SceneObjects::GetPosition(e).X, 2.0f + 12 * 0.02f * 4.0f));
    float minX = 100.0f, maxX = -100.0f;
    for (int i = 0; i < 150; ++i)
    {
        TestEnvironment::RunFrame(FRAME_MS);
        Vec3 p = SceneObjects::GetPosition(e);
        minX = std::min(minX, p.X);
        maxX = std::max(maxX, p.X);
        CHECK(Near(p.Z, 3.0f));
    }
    CHECK(Near(minX, 0.0f, 0.1f));
    CHECK(Near(maxX, 4.0f, 0.1f));
}

TEST_CASE("Game scripts: PlayerController moves kinematic and dynamic players")
{
    Setup();
    Entity kinematic = Player({-5, 0, 0}, BodyType::None);
    AppStub::Get().Keys[App::KEY_W] = true;
    Play(0.5f);
    CHECK(Near(SceneObjects::GetPosition(kinematic).Z, 2.5f, 0.05f));
    CHECK(Near(SceneObjects::GetPosition(kinematic).X, -5.0f));
    AppStub::Get().Keys[App::KEY_W] = false;
    AppStub::Get().Keys[App::KEY_D] = true;
    Play(0.2f);
    // D is screen right = -X (the camera looks along +Z)
    CHECK(SceneObjects::GetPosition(kinematic).X < -5.5f);
    AppStub::Reset();

    Setup();
    Entity dynamic = Player({0, 0, 0}, BodyType::Dynamic);
    AppStub::Get().Keys[App::KEY_UP] = true;
    Play(0.5f);
    CHECK(Near(ECS.GetComponent<RigidBody>(dynamic).Velocity.Y, 5.0f, 0.01f));
    CHECK(SceneObjects::GetPosition(dynamic).Z > 1.0f);
    AppStub::Get().Keys[App::KEY_UP] = false;
    Play(0.1f);
    CHECK(Near(ECS.GetComponent<RigidBody>(dynamic).Velocity.Y, 0.0f, 0.01f));
    AppStub::Reset();
}

TEST_CASE("Game scripts: Follower chases the player only within range")
{
    Setup();
    Player({0, 0, 0}, BodyType::None);
    Entity near = Scripted("Near", Shape2DType::Triangle, {4, 0, 0}, "Follower", {{"Speed", 2.0f}, {"Range", 6.0f}});
    Entity far = Scripted("Far", Shape2DType::Triangle, {0, 0, 10}, "Follower", {{"Speed", 2.0f}, {"Range", 6.0f}});
    Play(0.5f);
    CHECK(Near(SceneObjects::GetPosition(near).X, 3.0f, 0.05f));
    // Turned to face the player (-X = 270 degrees)
    CHECK(Near(SceneObjects::GetYaw(near), 270.0f, 0.5f));
    CHECK(Near(SceneObjects::GetPosition(far).Z, 10.0f));
}

TEST_CASE("Game scripts: Collectible scores for CollectGame and disappears")
{
    Setup();
    UseCollectGame(7.0f, 3.0f);
    Entity player = Player({0, 0, 0});
    Entity gem = Scripted("Gem", Shape2DType::Polygon, {0, 0, 4}, "Collectible", {{"Points", 25.0f}}, BodyType::Trigger, "Pickup");
    Scripted("Gem 2", Shape2DType::Polygon, {8, 0, 8}, "Collectible", {}, BodyType::Trigger, "Pickup");
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(Game() != nullptr);
    CHECK_EQ(Game()->PickupsLeft(), 2);
    CHECK_EQ(Game()->Lives(), 3);

    // Walk into the gem
    SceneObjects::SetPosition(player, {0, 0, 3.8f});
    Play(0.1f);
    CHECK(!ECS.IsEntityAlive(gem));
    CHECK_EQ(Game()->Score(), 25);
    CHECK_EQ(Game()->PickupsLeft(), 1);
    CHECK(Game()->GetState() == CollectGame::State::Playing);
    CHECK(AppStub::WasPrinted("Score 25"));

    // Something else touching a pickup does not collect it
    Entity rock = SceneObjects::CreateShape(Fixture::ShapeOf("Rock", Shape2DType::Circle, {8, 0, 8}, BodyType::Dynamic));
    Play(0.1f);
    CHECK_EQ(Game()->PickupsLeft(), 1);
    (void)rock;

    // Last pickup: level complete, then no level_8 file -> finished
    SceneObjects::SetPosition(player, {8, 0, 7.5f});
    Play(0.1f);
    CHECK_EQ(Game()->PickupsLeft(), 0);
    CHECK(Game()->GetState() == CollectGame::State::LevelComplete);
    CHECK(AppStub::WasPrinted("Level complete"));
    Play(2.2f);
    CHECK(Game()->GetState() == CollectGame::State::Finished);
    CHECK(AppStub::WasPrinted("You win"));
}

TEST_CASE("Game scripts: completing a level loads the next level file")
{
    Setup();
    UseCollectGame(1.0f, 3.0f);
    Entity player = Player({0, 0, 0});
    Scripted("Gem", Shape2DType::Polygon, {0, 0, 4}, "Collectible", {}, BodyType::Trigger, "Pickup");
    TestEnvironment::RunFrame(FRAME_MS);
    SceneObjects::SetPosition(player, {0, 0, 3.8f});
    Play(2.5f);
    // data/scenes/level_2.ubsave was loaded by the scene script
    CHECK_EQ(GameSceneManager.CurrentScenePath(), GameManager::ScenePath("level_2"));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->Name, std::string("level_2"));
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(Game() != nullptr);
    CHECK_EQ(Game()->Param("Level"), 2.0f);
    CHECK(Game()->PickupsLeft() > 0);
}

TEST_CASE("Game scripts: hazards cost lives, respawn the player and end the game")
{
    Setup();
    UseCollectGame(1.0f, 2.0f);
    Entity player = Player({0, 0, -5});
    Scripted("Spikes", Shape2DType::Rectangle, {0, 0, 5}, "Hazard", {}, BodyType::Trigger, "Hazard");
    Scripted("Gem", Shape2DType::Polygon, {9, 0, 9}, "Collectible", {}, BodyType::Trigger, "Pickup");
    std::string path = SaveAndPlay("hazard");
    player = SceneObjects::FindByName("Player");
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(Game() != nullptr);
    CHECK_EQ(Game()->Lives(), 2);

    SceneObjects::SetPosition(player, {0, 0, 5});
    Play(0.1f);
    CHECK_EQ(Game()->Lives(), 1);
    // Back at the start
    CHECK(Near(SceneObjects::GetPosition(player).Z, -5.0f, 0.2f));

    // Invulnerable for a second after a hit
    SceneObjects::SetPosition(player, {0, 0, 5});
    Play(0.1f);
    CHECK_EQ(Game()->Lives(), 1);
    SceneObjects::SetPosition(player, {0, 0, -5});
    Play(1.2f);
    SceneObjects::SetPosition(player, {0, 0, 5});
    Play(0.1f);
    CHECK_EQ(Game()->Lives(), 0);
    CHECK(Game()->GetState() == CollectGame::State::GameOver);
    CHECK(AppStub::WasPrinted("Out of lives"));

    // Game over restarts the scene from its file
    Play(2.2f);
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(Game() != nullptr);
    CHECK_EQ(Game()->Lives(), 2);
    CHECK(Game()->GetState() == CollectGame::State::Playing);
    CHECK_EQ(GameSceneManager.CurrentScenePath(), path);
    std::filesystem::remove(path);
}

TEST_CASE("Game scripts: without CollectGame a hazard restarts the scene")
{
    Setup();
    Player({0, 0, -5});
    Scripted("Spikes", Shape2DType::Rectangle, {0, 0, 5}, "Hazard", {}, BodyType::Trigger, "Hazard");
    std::string path = SaveAndPlay("restart");
    TestEnvironment::RunFrame(FRAME_MS);
    Entity player = SceneObjects::FindByName("Player");
    SceneObjects::SetPosition(player, {0, 0, 5});
    Play(0.1f);
    CHECK(Near(SceneObjects::GetPosition(SceneObjects::FindByName("Player")).Z, -5.0f, 0.2f));
    std::filesystem::remove(path);
}

TEST_CASE("Game scripts: Spawner fires projectiles that fly and expire")
{
    Setup();
    SceneObjects::ShapeDesc turret = Fixture::ShapeOf("Turret", Shape2DType::Triangle, {0, 0, 0}, BodyType::Static);
    turret.YawDegrees = 90.0f;
    turret.Script = "Spawner";
    turret.ScriptParams = {{"Interval", 0.5f}, {"Speed", 4.0f}, {"Lifetime", 0.8f}};
    SceneObjects::CreateShape(turret);

    Play(0.62f);
    Entity shot = SceneObjects::FindByName("Turret shot 1");
    REQUIRE(shot != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<SceneObject>(shot).Tag, std::string("Hazard"));
    CHECK(SceneObjects::GetBodyType(shot) == BodyType::Trigger);
    CHECK_EQ(ECS.GetComponent<ScriptComponent>(shot).Script, std::string("Projectile"));
    CHECK_EQ(ECS.GetComponent<ScriptComponent>(shot).Params.at("Speed"), 4.0f);
    // Flies along the turret's facing (+X)
    Vec3 p = SceneObjects::GetPosition(shot);
    CHECK(p.X > 0.2f);
    CHECK(Near(p.Z, 0.0f));

    Play(0.5f);
    CHECK(SceneObjects::FindByName("Turret shot 2") != NULL_ENTITY);
    // The first one expired after its lifetime
    Play(0.3f);
    CHECK(!ECS.IsEntityAlive(shot));
}

TEST_CASE("Game scripts: projectiles hurt the player and are used up")
{
    Setup();
    UseCollectGame(1.0f, 3.0f);
    // Contacts need a body on both sides
    Player({3, 0, 0}, BodyType::Dynamic);
    Scripted("Gem", Shape2DType::Polygon, {9, 0, 9}, "Collectible", {}, BodyType::Trigger, "Pickup");
    SceneObjects::ShapeDesc shot = Fixture::ShapeOf("Shot", Shape2DType::Circle, {0, 0, 0}, BodyType::Trigger);
    shot.YawDegrees = 90.0f;
    shot.Tag = "Hazard";
    shot.Script = "Projectile";
    shot.ScriptParams = {{"Speed", 10.0f}, {"Lifetime", 5.0f}};
    SceneObjects::CreateShape(shot);
    Play(0.6f);
    REQUIRE(Game() != nullptr);
    CHECK_EQ(Game()->Lives(), 2);
    CHECK(SceneObjects::FindByName("Shot") == NULL_ENTITY);
}
