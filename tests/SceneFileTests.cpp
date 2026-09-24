//---------------------------------------------------------------------------------
// SceneFileTests.cpp
//---------------------------------------------------------------------------------
//
// Verifies the scene files committed in data/scenes:
//
//   - every sample scene file still loads into exactly the world its authoring
//     script produces, and is byte-identical to a freshly authored file
//     (a golden test: any change to the save format or to what the prefabs
//     spawn shows up here)
//   - EVERY *.ubsave file in data/scenes, including scenes saved from the
//     editor GUI, loads without warnings, is playable, and re-saves to exactly
//     the same bytes
//
// If a change to the format or the prefabs is intentional, regenerate the
// sample scenes with:  cmake --build build/tests --target author_scenes
//
#include "Editor/SceneEditor.h"
#include "GameWorldFixture.h"
#include "SampleScenes.h"

#include <algorithm>
#include <filesystem>

#ifndef SCENES_DIR
#    error "SCENES_DIR must point to data/scenes"
#endif

using Fixture::Capture;
using Fixture::WorldImage;

namespace
{
    namespace fs = std::filesystem;

    const char* REGENERATE_HINT =
            " (regenerate with: cmake --build build/tests --target author_scenes)";

    fs::path ScenesDirectory()
    {
        return fs::path(SCENES_DIR);
    }

    std::vector<fs::path> SceneFiles()
    {
        std::vector<fs::path> files;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(ScenesDirectory(), ec))
        {
            if (entry.is_regular_file() &&
                entry.path().extension() == SampleScenes::SCENE_EXTENSION)
                files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    std::vector<std::uint8_t> ReadBytes(const fs::path& path)
    {
        std::vector<std::uint8_t> bytes;
        std::string error;
        Serialization::WorldSerializer::ReadFile(path.string(), bytes, error);
        return bytes;
    }
} // namespace

TEST_CASE("Scene files: every sample scene is committed")
{
    for (const auto& scene : SampleScenes::All())
    {
        fs::path path = ScenesDirectory() / (scene.Name + SampleScenes::SCENE_EXTENSION);
        if (!fs::exists(path))
        {
            TestFramework::ReportFailure(
                    __FILE__, __LINE__, "missing " + path.string() + REGENERATE_HINT);
        }
        ++TestFramework::TotalChecks();
    }
}

TEST_CASE("Scene files: committed sample scenes match their authoring scripts")
{
    Editor::SceneEditor editor;
    for (const auto& scene : SampleScenes::All())
    {
        fs::path path = ScenesDirectory() / (scene.Name + SampleScenes::SCENE_EXTENSION);
        if (!fs::exists(path))
            continue; // reported by the test above

        // Author the scene again with the current code
        Fixture::FreshWorld();
        scene.Author(editor);
        WorldImage authored = Capture(ECS);
        std::vector<std::uint8_t> authoredBytes = editor.SaveSceneToBytes(scene.Name);

        // Load the committed file
        Fixture::FreshWorld();
        editor.NewScene();
        Serialization::LoadResult result = editor.LoadScene(path.string());
        if (!result)
        {
            TestFramework::ReportFailure(
                    __FILE__, __LINE__, scene.Name + ": " + result.Error + REGENERATE_HINT);
            continue;
        }
        CHECK(result.Warnings.empty());
        CHECK_EQ(result.Metadata[Editor::SceneEditor::META_NAME], scene.Name);

        auto diffs = Fixture::Diff(authored, Capture(ECS));
        ++TestFramework::TotalChecks();
        if (!diffs.empty())
        {
            TestFramework::ReportFailure(__FILE__,
                                         __LINE__,
                                         scene.Name + " differs from its script:" +
                                                 Fixture::Describe(diffs) + REGENERATE_HINT);
        }

        ++TestFramework::TotalChecks();
        if (ReadBytes(path) != authoredBytes)
        {
            TestFramework::ReportFailure(__FILE__,
                                         __LINE__,
                                         scene.Name +
                                                 " is not byte-identical to a freshly "
                                                 "authored file" +
                                                 std::string(REGENERATE_HINT));
        }
    }
}

TEST_CASE("Scene files: every scene in data/scenes loads, is playable and re-saves identically")
{
    std::vector<fs::path> files = SceneFiles();
    REQUIRE(files.size() >= SampleScenes::All().size());

    Serialization::WorldSerializer serializer(Fixture::Registry());
    Editor::SceneEditor editor;
    for (const fs::path& path : files)
    {
        std::string name = path.filename().string();
        Fixture::FreshWorld();
        editor.NewScene();

        Serialization::LoadResult result = editor.LoadScene(path.string());
        ++TestFramework::TotalChecks();
        if (!result)
        {
            TestFramework::ReportFailure(__FILE__, __LINE__, name + ": " + result.Error);
            continue;
        }
        CHECK(result.Warnings.empty());
        CHECK(editor.Validate().empty());
        CHECK(!editor.EditableEntities().empty());

        // Saving the loaded world again must reproduce the file exactly
        std::vector<std::uint8_t> resaved = serializer.Save(ECS, result.Metadata);
        ++TestFramework::TotalChecks();
        if (resaved != ReadBytes(path))
            TestFramework::ReportFailure(
                    __FILE__, __LINE__, name + " does not re-save identically");

        // ... and survive a clear + load cycle unchanged
        WorldImage loaded = Capture(ECS);
        Fixture::FreshWorld();
        REQUIRE(serializer.Load(ECS, resaved).Success);
        CHECK_SAME_WORLD(loaded, Capture(ECS));
    }
}

TEST_CASE("Scene files: sample scenes contain what their descriptions promise")
{
    Editor::SceneEditor editor;
    auto load = [&](const std::string& name) {
        Fixture::FreshWorld();
        editor.NewScene();
        fs::path path = ScenesDirectory() / (name + SampleScenes::SCENE_EXTENSION);
        return editor.LoadScene(path.string()).Success;
    };
    auto count = [&](Editor::EntityKind kind) {
        size_t n = 0;
        for (Entity e : editor.EditableEntities())
            n += editor.KindOf(e) == kind ? 1 : 0;
        return n;
    };

    REQUIRE(load("empty_arena"));
    CHECK_EQ(editor.EditableEntities().size(), size_t(2));

    REQUIRE(load("first_contact"));
    CHECK_EQ(count(Editor::EntityKind::Soldier), size_t(9));
    CHECK_EQ(count(Editor::EntityKind::Support), size_t(2));
    CHECK_EQ(count(Editor::EntityKind::Crystal), size_t(3));
    CHECK_EQ(count(Editor::EntityKind::EnemySoldier), size_t(6));

    REQUIRE(load("fortress"));
    CHECK_EQ(count(Editor::EntityKind::Wall), size_t(4));
    CHECK_EQ(count(Editor::EntityKind::PlayerTank), size_t(1));
    CHECK_EQ(editor.GetStartingCrystals(), 60);
    size_t flipped = 0;
    for (Entity e : editor.EditableEntities())
    {
        if (editor.KindOf(e) == Editor::EntityKind::Wall &&
            ECS.GetComponent<AIObstacle>(e).Width == 5.0f)
            ++flipped;
    }
    CHECK_EQ(flipped, size_t(2));

    REQUIRE(load("tank_battle"));
    CHECK_EQ(count(Editor::EntityKind::PlayerTank), size_t(2));
    CHECK_EQ(count(Editor::EntityKind::EnemyTank), size_t(3));
    CHECK_EQ(editor.GetRoundNumber(), 3);

    REQUIRE(load("crystal_rush"));
    CHECK_EQ(count(Editor::EntityKind::Crystal), size_t(7));
    CHECK_EQ(count(Editor::EntityKind::EnemyTank), size_t(0)); // undone while authoring
    bool scoutFound = false;
    for (Entity e : editor.EditableEntities())
    {
        if (editor.KindOf(e) == Editor::EntityKind::Soldier)
        {
            scoutFound = true;
            CHECK_EQ(editor.GetHealth(e), 150);
            CHECK(Fixture::Same(editor.GetPosition(e).X, 5.5f));
        }
    }
    CHECK(scoutFound);
}
