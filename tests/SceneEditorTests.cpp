//---------------------------------------------------------------------------------
// SceneEditorTests.cpp
//---------------------------------------------------------------------------------
//
// Tests of the headless scene editor core (the logic behind the editor GUI):
// placing prefabs, picking, editing, undo / redo, validation and scene files
//
#include "Editor/SceneEditor.h"
#include "GameWorldFixture.h"
#include "Prefabs.h"

#include <cmath>
#include <filesystem>

using namespace Editor;
using Fixture::Capture;
using Fixture::WorldImage;

namespace
{
    // Fresh ECS + editor with a new scene
    struct EditorFixture
    {
        SceneEditor Editor;

        EditorFixture()
        {
            Fixture::FreshWorld();
            Editor.NewScene();
        }
    };

    size_t CountKind(SceneEditor& editor, EntityKind kind)
    {
        size_t count = 0;
        for (Entity e : ECS.GetLivingEntities())
        {
            if (editor.KindOf(e) == kind)
                ++count;
        }
        return count;
    }

    std::string TempScenePath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / ("ubisoft_next_editor_" + name)).string();
    }

    bool Near(float a, float b, float epsilon = 1e-4f)
    {
        return std::fabs(a - b) <= epsilon;
    }
} // namespace

TEST_CASE("Editor: a new scene is empty but playable")
{
    EditorFixture f;
    CHECK(f.Editor.Validate().empty());
    CHECK_EQ(CountKind(f.Editor, EntityKind::PlayerBase), size_t(1));
    CHECK_EQ(CountKind(f.Editor, EntityKind::Selector), size_t(1));
    CHECK_EQ(CountKind(f.Editor, EntityKind::Ground), size_t(1));
    CHECK_EQ(f.Editor.EditableEntities().size(), size_t(2));

    auto board = ECS.GetResource<BlackBoard>();
    CHECK(f.Editor.KindOf(board->UnitTarget) == EntityKind::Selector);
    CHECK(Near(board->UnitVectorField.HalfWidth, 25.0f));

    // Same round state as a new game of the main level
    auto state = ECS.GetResource<GameState>();
    CHECK_EQ(state->RoundNumber, 1);
    CHECK_EQ(state->PlayerCrystalInventory, 25);
    CHECK(state->currentState == Spawn);
    CHECK_EQ(state->ObstacleInCursor, NULL_ENTITY);

    CHECK_EQ(f.Editor.UndoCount(), size_t(0));
    CHECK(!f.Editor.IsDirty());
}

TEST_CASE("Editor: new scene resets state left over from a previous game")
{
    Fixture::BuildSampleLevel(); // mid game world, round 4, build mode ...
    SceneEditor editor;
    editor.NewScene();
    CHECK_EQ(ECS.GetResource<GameState>()->RoundNumber, 1);
    CHECK(ECS.GetResource<UIState>()->state == DefaultContext);
    // Every saved BlackBoard field is back to its default
    auto board = ECS.GetResource<BlackBoard>();
    CHECK(board->PlayerTankTargets.empty());
    CHECK(board->EnemyTankTargets.empty());
    CHECK(board->LastKnownLocation.empty());
    CHECK(board->InLineOfSight.empty());
    CHECK(board->PatrolTargets.empty());
    CHECK_EQ(board->EnemyVectorField.GridCountWidth, size_t(60));
    CHECK_EQ(board->UnitVectorField.GridCountHeight, size_t(60));
    CHECK_EQ(board->EnemyTarget, NULL_ENTITY);
    CHECK_EQ(editor.EditableEntities().size(), size_t(2));
    CHECK(editor.Validate().empty());
}

TEST_CASE("Editor: placed prefabs match what the game spawns")
{
    EditorFixture f;
    PrefabSettings settings;
    settings.Health = 90;
    settings.Battalion = 4;
    settings.CrystalAmount = 17;
    settings.EnemySpeed = 0.002f;

    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1}, settings);
    CHECK(f.Editor.KindOf(soldier) == EntityKind::Soldier);
    CHECK_EQ(ECS.GetComponent<PlayerControlUnit>(soldier).battalionId, 4);
    CHECK_EQ(ECS.GetComponent<PlayerControlUnit>(soldier).health, 90);
    CHECK(ECS.GetComponent<RigidBody>(soldier).Category == UnitCollider);
    CHECK(ECS.GetComponent<FragShaderTag>(soldier).FragAssetId == BlinnPhongID);
    CHECK(ECS.GetComponent<Transform>(soldier).Plane == XZ);

    Entity support = f.Editor.Place(PrefabType::Support, {2, 0, 1}, settings);
    CHECK(f.Editor.KindOf(support) == EntityKind::Support);

    Entity tank = f.Editor.Place(PrefabType::PlayerTank, {3, 0, 3}, settings);
    CHECK(f.Editor.KindOf(tank) == EntityKind::PlayerTank);
    CHECK(ECS.GetComponent<PlayerControlUnit>(tank).isTank);
    Entity cannon = Prefabs::FindChildWithMesh(tank, CannonTank);
    Entity tankBase = Prefabs::FindChildWithMesh(tank, BaseTank);
    REQUIRE(cannon != NULL_ENTITY);
    REQUIRE(tankBase != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Transform>(cannon).Parent, tank);
    // Tank children are part of the tank, not separate scene objects
    CHECK(!f.Editor.IsEditable(cannon));

    Entity enemy = f.Editor.Place(PrefabType::EnemySoldier, {10, 0, 10}, settings);
    CHECK(f.Editor.KindOf(enemy) == EntityKind::EnemySoldier);
    CHECK(Fixture::Same(ECS.GetComponent<BasicEnemyUnit>(enemy).Speed, 0.002f));

    Entity enemyTank = f.Editor.Place(PrefabType::EnemyTank, {12, 0, 10}, settings);
    CHECK(f.Editor.KindOf(enemyTank) == EntityKind::EnemyTank);
    CHECK(Prefabs::FindChildWithMesh(enemyTank, CannonTank) != NULL_ENTITY);

    Entity crystal = f.Editor.Place(PrefabType::Crystal, {-5, 0, -5}, settings);
    CHECK(f.Editor.KindOf(crystal) == EntityKind::Crystal);
    CHECK_EQ(ECS.GetComponent<CrystalDeposit>(crystal).AmountOfCrystal, 17);
    CHECK(ECS.HasComponent<AIObstacle>(crystal));

    Entity wall = f.Editor.Place(PrefabType::Wall, {-8, 0, 0});
    CHECK(f.Editor.KindOf(wall) == EntityKind::Wall);
    CHECK_EQ(ECS.GetComponent<RigidBody>(wall).InvMass(), 0.0f);
    CHECK(Fixture::Same(ECS.GetComponent<AIObstacle>(wall), AIObstacle{1, 5}));
    CHECK_EQ(ECS.GetComponent<PlayerControlUnit>(wall).battalionId, -1);

    PrefabSettings rotated;
    rotated.YawDegrees = 90.0f;
    Entity flippedWall = f.Editor.Place(PrefabType::Wall, {-8, 0, 8}, rotated);
    CHECK(Fixture::Same(ECS.GetComponent<AIObstacle>(flippedWall), AIObstacle{5, 1}));
    CHECK(Near(f.Editor.GetYaw(flippedWall), 90.0f, 0.05f));

    CHECK(f.Editor.Validate().empty());
    CHECK_EQ(f.Editor.Selected(), flippedWall);
}

TEST_CASE("Editor: placement is clamped to the play area")
{
    EditorFixture f;
    Entity e = f.Editor.Place(PrefabType::Soldier, {100, 0, -100});
    Vec3 p = f.Editor.GetPosition(e);
    CHECK(Near(p.X, 24.0f));
    CHECK(Near(p.Z, -24.0f));
    CHECK(f.Editor.Validate().empty());
}

TEST_CASE("Editor: picking finds the object under the cursor")
{
    EditorFixture f;
    Entity soldier = f.Editor.Place(PrefabType::Soldier, {5, 0, 5});
    Entity crystal = f.Editor.Place(PrefabType::Crystal, {-5, 0, 5});
    Entity tank = f.Editor.Place(PrefabType::PlayerTank, {5, 0, -5});

    CHECK_EQ(f.Editor.Pick({5.1f, 0, 4.9f}), soldier);
    CHECK_EQ(f.Editor.Pick({-5.5f, 0, 5.2f}), crystal);
    // Clicking a tank selects the tank, not one of its mesh children
    CHECK_EQ(f.Editor.Pick({5, 0, -5}), tank);
    // Empty ground (the ground mesh itself is not selectable)
    CHECK_EQ(f.Editor.Pick({-15, 0, -15}), NULL_ENTITY);
    // Base is selectable
    CHECK(f.Editor.KindOf(f.Editor.Pick({0.5f, 0, 0.5f})) == EntityKind::PlayerBase);
}

TEST_CASE("Editor: picking respects the orientation of long walls")
{
    EditorFixture f;
    Entity wall = f.Editor.Place(PrefabType::Wall, {-10, 0, 0}); // 1 x 10 along Z
    CHECK_EQ(f.Editor.Pick({-10, 0, 4.5f}), wall);
    CHECK_EQ(f.Editor.Pick({-6, 0, 0}), NULL_ENTITY);

    f.Editor.SetYaw(wall, 90.0f); // now along X
    CHECK_EQ(f.Editor.Pick({-10, 0, 4.5f}), NULL_ENTITY);
    CHECK_EQ(f.Editor.Pick({-6, 0, 0}), wall);
}

TEST_CASE("Editor: overlapping objects pick the closest one")
{
    EditorFixture f;
    Entity a = f.Editor.Place(PrefabType::Soldier, {5.0f, 0, 5.0f});
    Entity b = f.Editor.Place(PrefabType::Soldier, {5.3f, 0, 5.0f});
    CHECK_EQ(f.Editor.Pick({5.05f, 0, 5.0f}), a);
    CHECK_EQ(f.Editor.Pick({5.28f, 0, 5.0f}), b);
}

TEST_CASE("Editor: moving a tank moves its children")
{
    EditorFixture f;
    Entity tank = f.Editor.Place(PrefabType::PlayerTank, {0, 0, -6});
    Entity cannon = Prefabs::FindChildWithMesh(tank, CannonTank);
    REQUIRE(f.Editor.Move(tank, {7, 0, 8}));
    Vec3 world = ECS.GetComponent<Transform>(cannon).GetWorldPosition();
    CHECK(Near(world.X, 7.0f));
    CHECK(Near(world.Z, 8.0f));
    CHECK(ECS.GetComponent<Transform>(cannon).IsDirty);
    CHECK(f.Editor.IsDirty());
}

TEST_CASE("Editor: moving keeps the height of floating objects")
{
    EditorFixture f;
    Entity selector = ECS.GetResource<BlackBoard>()->UnitTarget;
    float height = f.Editor.GetPosition(selector).Y;
    REQUIRE(f.Editor.Move(selector, {-3, 0, -3}));
    CHECK(Near(f.Editor.GetPosition(selector).Y, height));
    CHECK(Near(f.Editor.GetPosition(selector).X, -3.0f));
}

TEST_CASE("Editor: rotation of units and walls")
{
    EditorFixture f;
    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    REQUIRE(f.Editor.SetYaw(soldier, 45.0f));
    CHECK(Near(f.Editor.GetYaw(soldier), 45.0f, 0.05f));
    f.Editor.SetYaw(soldier, -90.0f);
    CHECK(Near(f.Editor.GetYaw(soldier), 270.0f, 0.05f));

    Entity wall = f.Editor.Place(PrefabType::Wall, {-5, 0, 0});
    f.Editor.SetYaw(wall, 80.0f); // snaps to 90
    CHECK(Near(f.Editor.GetYaw(wall), 90.0f, 0.05f));
    CHECK(Fixture::Same(ECS.GetComponent<AIObstacle>(wall), AIObstacle{5, 1}));
    f.Editor.SetYaw(wall, 185.0f); // snaps back to unflipped
    CHECK(Fixture::Same(ECS.GetComponent<AIObstacle>(wall), AIObstacle{1, 5}));
}

TEST_CASE("Editor: removing objects")
{
    EditorFixture f;
    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    Entity tank = f.Editor.Place(PrefabType::EnemyTank, {8, 0, 8});
    Entity cannon = Prefabs::FindChildWithMesh(tank, CannonTank);
    size_t before = ECS.GetEntityCount();

    CHECK(f.Editor.Remove(soldier));
    CHECK(!ECS.IsEntityAlive(soldier));
    CHECK(f.Editor.Remove(tank));
    CHECK(!ECS.IsEntityAlive(tank));
    CHECK(!ECS.IsEntityAlive(cannon));
    CHECK_EQ(ECS.GetEntityCount(), before - 4);
    CHECK_EQ(f.Editor.Selected(), NULL_ENTITY);

    // The game needs these, they can not be deleted
    Entity base = f.Editor.Pick({0, 0, 0});
    CHECK(!f.Editor.Remove(base));
    CHECK(!f.Editor.Remove(ECS.GetResource<BlackBoard>()->UnitTarget));
    CHECK(f.Editor.Validate().empty());
}

TEST_CASE("Editor: property editing")
{
    EditorFixture f;
    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    Entity enemy = f.Editor.Place(PrefabType::EnemySoldier, {9, 0, 9});
    Entity crystal = f.Editor.Place(PrefabType::Crystal, {-9, 0, 9});
    Entity base = f.Editor.Pick({0, 0, 0});

    CHECK(f.Editor.SetHealth(soldier, 140));
    CHECK_EQ(f.Editor.GetHealth(soldier), 140);
    CHECK(f.Editor.SetHealth(soldier, -20)); // clamped
    CHECK_EQ(f.Editor.GetHealth(soldier), 1);
    CHECK(f.Editor.SetHealth(base, 1500));
    CHECK_EQ(ECS.GetComponent<PlayerBaseComponent>(base).PlayerBaseHealth, 1500);
    CHECK(f.Editor.SetHealth(enemy, 70));
    CHECK_EQ(ECS.GetComponent<BasicEnemyUnit>(enemy).health, 70);
    CHECK(!f.Editor.SetHealth(crystal, 10));
    CHECK_EQ(f.Editor.GetHealth(crystal), -1);

    CHECK(f.Editor.SetBattalion(soldier, 6));
    CHECK_EQ(f.Editor.GetBattalion(soldier), 6);
    CHECK(!f.Editor.SetBattalion(enemy, 2));

    CHECK(f.Editor.SetCrystalAmount(crystal, 0)); // clamped, 0 would vanish in game
    CHECK_EQ(f.Editor.GetCrystalAmount(crystal), 1);
    CHECK(!f.Editor.SetCrystalAmount(soldier, 5));

    CHECK(f.Editor.SetEnemySpeed(enemy, 0.003f));
    CHECK(Fixture::Same(f.Editor.GetEnemySpeed(enemy), 0.003f));
    CHECK(!f.Editor.SetEnemySpeed(enemy, 0.0f));
    CHECK(!f.Editor.SetEnemySpeed(soldier, 0.003f));

    f.Editor.SetStartingCrystals(80);
    f.Editor.SetRoundNumber(0); // clamped
    f.Editor.SetSpawnVolume(6);
    CHECK_EQ(f.Editor.GetStartingCrystals(), 80);
    CHECK_EQ(f.Editor.GetRoundNumber(), 1);
    CHECK_EQ(f.Editor.GetSpawnVolume(), 6);
}

TEST_CASE("Editor: every edit can be undone and redone exactly")
{
    EditorFixture f;
    std::vector<WorldImage> history = {Capture(ECS)};

    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    history.push_back(Capture(ECS));
    f.Editor.Move(soldier, {4, 0, -2});
    history.push_back(Capture(ECS));
    f.Editor.SetHealth(soldier, 55);
    history.push_back(Capture(ECS));
    Entity tank = f.Editor.Place(PrefabType::PlayerTank, {-6, 0, -6});
    history.push_back(Capture(ECS));
    f.Editor.SetYaw(tank, 30.0f);
    history.push_back(Capture(ECS));
    f.Editor.Remove(soldier);
    history.push_back(Capture(ECS));
    f.Editor.SetStartingCrystals(99);
    history.push_back(Capture(ECS));

    // Undo walks back through every state
    for (size_t i = history.size() - 1; i-- > 0;)
    {
        REQUIRE(f.Editor.Undo());
        CHECK_SAME_WORLD(history[i], Capture(ECS));
    }
    CHECK(!f.Editor.Undo());

    // Redo walks forward again
    for (size_t i = 1; i < history.size(); ++i)
    {
        REQUIRE(f.Editor.Redo());
        CHECK_SAME_WORLD(history[i], Capture(ECS));
    }
    CHECK(!f.Editor.Redo());
}

TEST_CASE("Editor: a new edit after undo discards the redo history")
{
    EditorFixture f;
    f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    f.Editor.Place(PrefabType::Soldier, {2, 0, 1});
    f.Editor.Undo();
    CHECK_EQ(f.Editor.RedoCount(), size_t(1));
    f.Editor.Place(PrefabType::Crystal, {-4, 0, 4});
    CHECK_EQ(f.Editor.RedoCount(), size_t(0));
    CHECK(!f.Editor.Redo());
}

TEST_CASE("Editor: undo history is bounded and clears selection of undone objects")
{
    EditorFixture f;
    for (size_t i = 0; i < SceneEditor::MAX_UNDO + 10; ++i)
        f.Editor.Place(PrefabType::Crystal, {-20.0f + 0.5f * i, 0, 10});
    CHECK_EQ(f.Editor.UndoCount(), SceneEditor::MAX_UNDO);

    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    CHECK_EQ(f.Editor.Selected(), soldier);
    f.Editor.Undo();
    CHECK_EQ(f.Editor.Selected(), NULL_ENTITY);
}

TEST_CASE("Editor: the world replaced callback fires for new, load, undo and redo")
{
    EditorFixture f;
    int calls = 0;
    f.Editor.OnWorldReplaced = [&] { ++calls; };
    f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    CHECK_EQ(calls, 0); // regular edits keep entities, render caches stay valid
    f.Editor.Undo();
    f.Editor.Redo();
    CHECK_EQ(calls, 2);
    std::string path = TempScenePath("callback.ubsave");
    REQUIRE(f.Editor.SaveScene(path, "callback").Success);
    REQUIRE(f.Editor.LoadScene(path).Success);
    f.Editor.NewScene();
    CHECK_EQ(calls, 4);
    std::filesystem::remove(path);
}

TEST_CASE("Editor: validation reports scenes the game can not play")
{
    EditorFixture f;
    Entity base = f.Editor.Pick({0, 0, 0});
    ECS.DestroyEntity(base); // bypasses the editor protection
    auto issues = f.Editor.Validate();
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].find("player base") != std::string::npos);

    f.Editor.NewScene();
    Prefabs::SpawnUnitSelector({5, 2, 5});
    CHECK(f.Editor.Validate().size() == 1);

    f.Editor.NewScene();
    Entity soldier = f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    ECS.GetComponent<Transform>(soldier).SetLocalPosition({40, 0, 0});
    issues = f.Editor.Validate();
    REQUIRE(issues.size() == 1);
    CHECK(issues[0].find("outside the play area") != std::string::npos);

    // Invalid scenes are not written to disk
    std::string path = TempScenePath("invalid.ubsave");
    std::filesystem::remove(path);
    Serialization::SaveResult result = f.Editor.SaveScene(path, "invalid");
    CHECK(!result.Success);
    CHECK(!std::filesystem::exists(path));
}

TEST_CASE("Editor: save and load a scene file")
{
    EditorFixture f;
    f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    f.Editor.Place(PrefabType::EnemyTank, {10, 0, 12});
    f.Editor.Place(PrefabType::Wall, {-6, 0, 0});
    f.Editor.SetStartingCrystals(70);
    WorldImage authored = Capture(ECS);

    std::string path = TempScenePath("roundtrip.ubsave");
    Serialization::SaveResult saved = f.Editor.SaveScene(path, "Round Trip");
    REQUIRE(saved.Success);
    CHECK(!f.Editor.IsDirty());

    f.Editor.NewScene();
    f.Editor.Place(PrefabType::Crystal, {3, 0, 3});
    Serialization::LoadResult loaded = f.Editor.LoadScene(path);
    REQUIRE(loaded.Success);
    CHECK(loaded.Warnings.empty());
    CHECK_EQ(loaded.Metadata[SceneEditor::META_NAME], std::string("Round Trip"));
    CHECK_EQ(loaded.Metadata[SceneEditor::META_SCENE], std::string("MainLevel"));
    CHECK_SAME_WORLD(authored, Capture(ECS));
    CHECK_EQ(f.Editor.UndoCount(), size_t(0));
    CHECK(!f.Editor.IsDirty());
    std::filesystem::remove(path);
}

TEST_CASE("Editor: loading a file that is not a playable scene keeps the current scene")
{
    EditorFixture f;
    f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    WorldImage current = Capture(ECS);

    // A save that belongs to another scene
    std::string otherScene = TempScenePath("title.ubsave");
    Serialization::WorldSerializer serializer(Fixture::Registry());
    REQUIRE(serializer.SaveToFile(ECS, otherScene, {{"Scene", "TitleScreen"}}).Success);
    Serialization::LoadResult result = f.Editor.LoadScene(otherScene);
    CHECK(!result.Success);
    CHECK(result.Error.find("main level") != std::string::npos);
    CHECK_SAME_WORLD(current, Capture(ECS));

    // A main level save without a player base
    Entity base = f.Editor.Pick({0, 0, 0});
    ECS.DestroyEntity(base);
    std::string noBase = TempScenePath("nobase.ubsave");
    REQUIRE(serializer.SaveToFile(ECS, noBase, {{"Scene", "MainLevel"}}).Success);
    f.Editor.NewScene();
    f.Editor.Place(PrefabType::Soldier, {1, 0, 1});
    current = Capture(ECS);
    result = f.Editor.LoadScene(noBase);
    CHECK(!result.Success);
    CHECK(result.Error.find("player base") != std::string::npos);
    CHECK_SAME_WORLD(current, Capture(ECS));

    // Missing file
    CHECK(!f.Editor.LoadScene(TempScenePath("missing.ubsave")).Success);
    CHECK_SAME_WORLD(current, Capture(ECS));

    std::filesystem::remove(otherScene);
    std::filesystem::remove(noBase);
}

TEST_CASE("Editor: every authored object is recognised when the game restores AI")
{
    // RestoreMainLevelRuntimeState identifies units by the same components the
    // editor uses; nothing placed may end up unrecognised ("Other")
    EditorFixture f;
    for (int t = 0; t < static_cast<int>(PrefabType::Count); ++t)
        f.Editor.Place(static_cast<PrefabType>(t), {-12.0f + 4.0f * t, 0, 8});
    for (Entity e : ECS.GetLivingEntities())
    {
        EntityKind kind = f.Editor.KindOf(e);
        if (kind == EntityKind::Other)
        {
            // Only tank meshes may be "Other" and they must have a tank parent
            Entity parent = ECS.GetComponent<Transform>(e).Parent;
            CHECK(parent != NULL_ENTITY);
            EntityKind parentKind = f.Editor.KindOf(parent);
            CHECK((parentKind == EntityKind::PlayerTank || parentKind == EntityKind::EnemyTank));
        }
    }
}
