//---------------------------------------------------------------------------------
// ScriptingTests.cpp
//---------------------------------------------------------------------------------
//
// C++ scripts on top of the ECS: the registry, the ScriptSystem lifecycle,
// scene scripts, collision callbacks from the real physics system, input and
// scene requests. Frames are run through the real GameManager
//
#include "AppStub.h"
#include "Input.h"
#include "Scripting/ScriptRegistry.h"
#include "Scripting/ScriptSystem.h"
#include "WorldFixture.h"

#include <algorithm>

using SceneObjects::BodyType;

namespace
{
    std::vector<std::string>& Log()
    {
        static std::vector<std::string> log;
        return log;
    }

    bool Logged(const std::string& line)
    {
        return std::find(Log().begin(), Log().end(), line) != Log().end();
    }

    size_t CountLogged(const std::string& line)
    {
        return static_cast<size_t>(std::count(Log().begin(), Log().end(), line));
    }

    class ProbeScene : public SceneScript
    {
      public:
        void OnStart() override { Log().push_back("scene start " + std::to_string(Param("Level"))); }
        void OnUpdate(float) override { ++Updates; }
        void OnRender() override { DrawText(10, 10, "probe hud"); }
        void OnDestroy() override { Log().push_back("scene destroy"); }
        int Updates = 0;
    };

    class Probe : public Script
    {
      public:
        void OnStart() override
        {
            // Kept: the object may already be gone when OnDestroy runs
            m_Name = NameOf(Self());
            Log().push_back("start " + m_Name);
        }
        void OnUpdate(float deltaSeconds) override
        {
            ++Updates;
            LastDelta = deltaSeconds;
            if (KeyPressed(App::KEY_SPACE))
                Log().push_back("space pressed " + NameOf(Self()));
            if (SceneScriptAs<ProbeScene>() != nullptr)
                SawScene = true;
        }
        void OnDestroy() override { Log().push_back("destroy " + m_Name); }
        void OnCollisionEnter(Entity other) override { Log().push_back("enter " + NameOf(Self()) + " " + NameOf(other)); }
        void OnCollisionExit(Entity other) override { Log().push_back("exit " + NameOf(Self()) + " " + NameOf(other)); }
        int Updates = 0;
        float LastDelta = 0.0f;
        bool SawScene = false;

      private:
        std::string m_Name;
    };

    // Spawns one scripted object on its first update
    class Maker : public Script
    {
      public:
        void OnUpdate(float) override
        {
            if (m_Done)
                return;
            m_Done = true;
            SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Made", Shape2DType::Circle, {3, 0, 3});
            desc.Script = "Probe";
            Spawn(desc);
        }

      private:
        bool m_Done = false;
    };

    // Asks for a scene change from inside a script
    class Loader : public Script
    {
      public:
        void OnUpdate(float) override { LoadScene("level_1"); }
    };

    void RegisterProbes()
    {
        ScriptRegistry& r = ScriptRegistry::Get();
        r.Register<Probe>("Probe", "test probe", {{"Speed", 1.0f, 0.5f}});
        r.Register<ProbeScene>("ProbeScene", "test scene probe", {{"Level", 1.0f, 1.0f}});
        r.Register<Maker>("Maker", "spawns a probe");
        r.Register<Loader>("Loader", "loads level_1");
    }

    // Empty simulated world (ScenePlayer) with the probes registered
    void Setup()
    {
        RegisterProbes();
        Fixture::FreshWorld();
        AppStub::Reset();
        Log().clear();
    }

    Entity ProbeObject(const std::string& name, const Vec3& position, BodyType body = BodyType::None)
    {
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf(name, Shape2DType::Circle, position, body);
        desc.Script = "Probe";
        return SceneObjects::CreateShape(desc);
    }

    Probe* ProbeOf(Entity e) { return dynamic_cast<Probe*>(GameSceneManager.Scripts().GetScript(e)); }

    constexpr float FRAME_MS = 20.0f;
} // namespace

TEST_CASE("Scripts: registry knows every game script and its parameters")
{
    const ScriptRegistry& r = ScriptRegistry::Get();
    std::vector<std::string> objects = r.Names(false);
    std::vector<std::string> scenes = r.Names(true);
    for (const char* name : {"Rotator", "Patrol", "Mover", "PlayerController", "Follower",
                             "Collectible", "Hazard", "MovingHazard", "Projectile", "Spawner"})
    {
        CHECK(std::find(objects.begin(), objects.end(), name) != objects.end());
        CHECK(std::find(scenes.begin(), scenes.end(), name) == scenes.end());
    }
    CHECK(std::find(scenes.begin(), scenes.end(), "CollectGame") != scenes.end());
    CHECK(std::find(objects.begin(), objects.end(), "CollectGame") == objects.end());
    CHECK(std::is_sorted(objects.begin(), objects.end()));

    const ScriptInfo* rotator = r.Find("Rotator");
    REQUIRE(rotator != nullptr);
    REQUIRE(rotator->Params.size() == 1);
    CHECK_EQ(rotator->Params[0].Name, std::string("Speed"));
    CHECK(r.Find("NoSuchScript") == nullptr);

    // Declared defaults, overridden by the scene's values
    auto params = r.ResolveParams("Spawner", {{"Speed", 1.0f}});
    CHECK_EQ(params.size(), size_t(3));
    CHECK_EQ(params.at("Speed"), 1.0f);
    CHECK_EQ(params.at("Interval"), 2.0f);
}

TEST_CASE("Scripts: an object script starts, updates every frame and is destroyed with its object")
{
    Setup();
    Entity a = ProbeObject("A", {0, 0, 0});
    Entity b = ProbeObject("B", {2, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Logged("start A"));
    CHECK(Logged("start B"));
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(2));
    REQUIRE(ProbeOf(a) != nullptr);
    CHECK_EQ(ProbeOf(a)->Updates, 1);
    CHECK_EQ(ProbeOf(a)->LastDelta, FRAME_MS / 1000.0f);
    CHECK_EQ(ProbeOf(a)->ScriptName(), std::string("Probe"));
    CHECK_EQ(ProbeOf(a)->Param("Speed"), 1.0f);

    TestEnvironment::RunFrames(4, FRAME_MS);
    CHECK_EQ(ProbeOf(a)->Updates, 5);
    CHECK_EQ(CountLogged("start A"), size_t(1));

    // Destroying the entity destroys its script
    SceneObjects::Destroy(a);
    ECS.FlushECS();
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Logged("destroy A"));
    CHECK(GameSceneManager.Scripts().GetScript(a) == nullptr);
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(1));

    // Removing the component too
    ECS.RemoveComponent<ScriptComponent>(b);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Logged("destroy B"));
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(0));
}

TEST_CASE("Scripts: changing the script name or parameters restarts the right instance")
{
    Setup();
    Entity e = ProbeObject("A", {0, 0, 0});
    ECS.GetComponent<ScriptComponent>(e).Params["Speed"] = 4.0f;
    TestEnvironment::RunFrame(FRAME_MS);
    REQUIRE(ProbeOf(e) != nullptr);
    CHECK_EQ(ProbeOf(e)->Param("Speed"), 4.0f);

    ECS.GetComponent<ScriptComponent>(e).Script = "Rotator";
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Logged("destroy A"));
    ScriptBase* script = GameSceneManager.Scripts().GetScript(e);
    REQUIRE(script != nullptr);
    CHECK_EQ(script->ScriptName(), std::string("Rotator"));
    // Unknown parameters (Speed exists on Rotator too) are passed through
    CHECK_EQ(script->Param("Speed"), 4.0f);
}

TEST_CASE("Scripts: unknown scripts and wrong kinds are reported, not run")
{
    Setup();
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Ghost", Shape2DType::Circle, {0, 0, 0});
    desc.Script = "DoesNotExist";
    SceneObjects::CreateShape(desc);
    desc.Name = "Wrong";
    desc.Script = "CollectGame"; // a scene script on an object
    SceneObjects::CreateShape(desc);
    ECS.GetResource<SceneSettings>()->SceneScript = "Rotator"; // an object script on the scene

    TestEnvironment::RunFrames(3, FRAME_MS);
    const auto& missing = GameSceneManager.Scripts().MissingScripts();
    CHECK_EQ(missing.size(), size_t(3));
    CHECK(std::find(missing.begin(), missing.end(), "DoesNotExist") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "CollectGame") != missing.end());
    CHECK(std::find(missing.begin(), missing.end(), "Rotator") != missing.end());
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(0));
    CHECK(GameSceneManager.Scripts().GetSceneScript() == nullptr);
}

TEST_CASE("Scripts: the scene script runs first and object scripts can reach it")
{
    Setup();
    auto settings = ECS.GetResource<SceneSettings>();
    settings->SceneScript = "ProbeScene";
    settings->SceneParams = {{"Level", 3.0f}};
    Entity e = ProbeObject("A", {0, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);

    CHECK(Logged("scene start 3.000000"));
    auto* scene = dynamic_cast<ProbeScene*>(GameSceneManager.Scripts().GetSceneScript());
    REQUIRE(scene != nullptr);
    CHECK_EQ(scene->Updates, 1);
    CHECK(ProbeOf(e)->SawScene);
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(2));
    // Scene script HUD is drawn after the 3D scene
    CHECK(AppStub::WasPrinted("probe hud"));

    // Switching the scene script replaces the instance
    settings->SceneScript.clear();
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Logged("scene destroy"));
    CHECK(GameSceneManager.Scripts().GetSceneScript() == nullptr);
}

TEST_CASE("Scripts: scripts only run while the active scene simulates the world")
{
    RegisterProbes();
    Log().clear();
    GameSceneManager.SetActiveScene(TestEnvironment::EDITOR_SCENE);
    Entity e = ProbeObject("A", {0, 0, 0});
    TestEnvironment::RunFrames(3, FRAME_MS);
    CHECK(!Logged("start A"));
    CHECK(GameSceneManager.Scripts().GetScript(e) == nullptr);
    GameSceneManager.SetActiveScene(ScenePlayer::NAME);
}

TEST_CASE("Scripts: switching scenes destroys every script")
{
    Setup();
    ProbeObject("A", {0, 0, 0});
    ECS.GetResource<SceneSettings>()->SceneScript = "ProbeScene";
    TestEnvironment::RunFrame(FRAME_MS);
    GameSceneManager.SetActiveScene(ScenePlayer::NAME);
    CHECK(Logged("destroy A"));
    CHECK(Logged("scene destroy"));
    CHECK_EQ(GameSceneManager.Scripts().InstanceCount(), size_t(0));
}

TEST_CASE("Scripts: collision enter / exit come from the real physics system")
{
    Setup();
    // A dynamic ball resting inside a trigger area, and a far away probe
    Entity ball = ProbeObject("Ball", {0, 0, 0}, BodyType::Dynamic);
    SceneObjects::ShapeDesc zone = Fixture::ShapeOf("Zone", Shape2DType::Rectangle, {0, 0, 0}, BodyType::Trigger);
    zone.Shape.Width = 4.0f;
    zone.Shape.Height = 4.0f;
    zone.Script = "Probe";
    Entity area = SceneObjects::CreateShape(zone);
    ProbeObject("Far", {15, 0, 15}, BodyType::Dynamic);

    TestEnvironment::RunFrames(3, FRAME_MS);
    CHECK_EQ(CountLogged("enter Ball Zone"), size_t(1));
    CHECK_EQ(CountLogged("enter Zone Ball"), size_t(1));
    CHECK(!Logged("enter Far Zone"));
    CHECK(!Logged("exit Ball Zone"));

    // Triggers do not push: the ball is still where it was
    Vec3 p = SceneObjects::GetPosition(ball);
    CHECK(std::fabs(p.X) < 1e-3f);
    CHECK(std::fabs(p.Z) < 1e-3f);

    // Leaving the zone (physics reads positions from the Transform)
    SceneObjects::SetPosition(ball, {10, 0, 0});
    TestEnvironment::RunFrames(3, FRAME_MS);
    CHECK_EQ(CountLogged("exit Ball Zone"), size_t(1));
    CHECK_EQ(CountLogged("exit Zone Ball"), size_t(1));
    (void)area;
}

TEST_CASE("Scripts: solid bodies collide, triggers let bodies through")
{
    Setup();
    auto launch = [](const std::string& name, float z) {
        SceneObjects::ShapeDesc ball = Fixture::ShapeOf(name, Shape2DType::Circle, {-4, 0, z}, BodyType::Dynamic);
        Entity e = SceneObjects::CreateShape(ball);
        ECS.GetComponent<RigidBody>(e).Velocity = Vec2(8.0f, 0.0f);
        return e;
    };
    SceneObjects::ShapeDesc wall = Fixture::ShapeOf("Wall", Shape2DType::Rectangle, {0, 0, 0}, BodyType::Static);
    wall.Shape.Width = 0.5f;
    wall.Shape.Height = 2.0f;
    SceneObjects::CreateShape(wall);
    SceneObjects::ShapeDesc gate = wall;
    gate.Name = "Gate";
    gate.Position = {0, 0, 6};
    gate.Body = BodyType::Trigger;
    SceneObjects::CreateShape(gate);

    Entity blocked = launch("Blocked", 0.0f);
    Entity passed = launch("Passed", 6.0f);
    TestEnvironment::RunFrames(60, FRAME_MS);
    // Ball radius 0.5 + half the wall 0.25: stopped before x = -0.75, or
    // completely through past x = 0.75 (the bodies are damped, both slow down)
    CHECK(SceneObjects::GetPosition(blocked).X < -0.7f);
    CHECK(SceneObjects::GetPosition(passed).X > 0.8f);
}

TEST_CASE("Scripts: objects spawned by a script get their own script next frame")
{
    Setup();
    SceneObjects::ShapeDesc maker = Fixture::ShapeOf("Maker", Shape2DType::Rectangle, {0, 0, 0});
    maker.Script = "Maker";
    SceneObjects::CreateShape(maker);
    TestEnvironment::RunFrame(FRAME_MS);
    Entity made = SceneObjects::FindByName("Made");
    REQUIRE(made != NULL_ENTITY);
    CHECK(!Logged("start Made"));
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Logged("start Made"));
    REQUIRE(ProbeOf(made) != nullptr);
    CHECK_EQ(ProbeOf(made)->Updates, 1);
    // Spawned objects are ordinary scene objects: they are rendered too
    CHECK(ECS.GetComponent<Shape2D>(made).Built);
}

TEST_CASE("Input: pressed is reported for one frame, down while held")
{
    Setup();
    ProbeObject("A", {0, 0, 0});
    TestEnvironment::RunFrame(FRAME_MS);
    AppStub::Get().Keys[App::KEY_SPACE] = true;
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Input::IsDown(App::KEY_SPACE));
    CHECK(Input::WasPressed(App::KEY_SPACE));
    TestEnvironment::RunFrames(3, FRAME_MS);
    CHECK(Input::IsDown(App::KEY_SPACE));
    CHECK(!Input::WasPressed(App::KEY_SPACE));
    CHECK_EQ(CountLogged("space pressed A"), size_t(1));

    AppStub::Get().Keys[App::KEY_SPACE] = false;
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(Input::WasReleased(App::KEY_SPACE));
    CHECK(!Input::IsDown(App::KEY_SPACE));
    AppStub::Get().Keys[App::KEY_SPACE] = true;
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK_EQ(CountLogged("space pressed A"), size_t(2));
    AppStub::Reset();
}

TEST_CASE("Scripts: LoadScene from a script replaces the world on the next frame")
{
    Setup();
    SceneObjects::ShapeDesc loader = Fixture::ShapeOf("Loader", Shape2DType::Rectangle, {0, 0, 0});
    loader.Script = "Loader";
    SceneObjects::CreateShape(loader);
    TestEnvironment::RunFrame(FRAME_MS);
    // The request is handled between frames, never in the middle of an update
    CHECK(SceneObjects::FindByName("Loader") != NULL_ENTITY);
    TestEnvironment::RunFrame(FRAME_MS);
    CHECK(SceneObjects::FindByName("Loader") == NULL_ENTITY);
    CHECK_EQ(GameSceneManager.GetActiveScene(), std::string(ScenePlayer::NAME));
    CHECK_EQ(GameSceneManager.CurrentScenePath(), GameManager::ScenePath("level_1"));
    CHECK_EQ(ECS.GetResource<SceneSettings>()->SceneScript, std::string("CollectGame"));
    CHECK(!SceneObjects::FindByTag("Pickup").empty());
}

TEST_CASE("Scenes: a fresh scene has no file, restarting it keeps the world")
{
    std::string error;
    REQUIRE(GameSceneManager.LoadGame(GameManager::ScenePath("level_1"), error));
    CHECK_EQ(GameSceneManager.CurrentScenePath(), GameManager::ScenePath("level_1"));

    Setup();
    CHECK(GameSceneManager.CurrentScenePath().empty());
    Entity e = ProbeObject("A", {0, 0, 0});
    GameSceneManager.RequestRestart();
    TestEnvironment::RunFrame(FRAME_MS);
    // Nothing to reload: the previous scene's file is not loaded instead
    CHECK(ECS.IsEntityAlive(e));
    CHECK(SceneObjects::FindByTag("Pickup").empty());
}
