//---------------------------------------------------------------------------------
// SceneFileTests.cpp
//---------------------------------------------------------------------------------
//
// The sample scenes committed in data/scenes: each one must load, validate,
// match what tests/scenes/SampleScenes.cpp authors today, and play
//
#include "SampleScenes.h"
#include "SceneEditor.h"
#include "Scripting/ScriptSystem.h"
#include "WorldFixture.h"

#include <filesystem>

using Fixture::Capture;

TEST_CASE("Scene files: every sample scene is committed and loads cleanly")
{
    Editor::SceneEditor editor;
    for (const auto& scene : SampleScenes::All())
    {
        std::string path = GameManager::ScenePath(scene.Name);
        REQUIRE(std::filesystem::exists(path));
        Fixture::FreshWorld();
        Serialization::LoadResult result = editor.LoadScene(path);
        REQUIRE(result.Success);
        CHECK(result.Warnings.empty());
        CHECK_EQ(result.Metadata["Name"], scene.Name);
        CHECK(editor.Validate().empty());
    }
}

TEST_CASE("Scene files: committed files match the authoring code (run author_scenes if not)")
{
    Editor::SceneEditor editor;
    for (const auto& scene : SampleScenes::All())
    {
        Fixture::FreshWorld();
        scene.Author(editor);
        Serialization::WorldSerializer::SetFileFormat(SampleScenes::FormatOf(scene));
        std::vector<std::uint8_t> expected = editor.SaveSceneToBytes(scene.Name);
        Serialization::WorldSerializer::SetFileFormat(Serialization::SaveFormat::Binary);
        Fixture::WorldImage authored = Capture();

        Fixture::FreshWorld();
        REQUIRE(editor.LoadScene(GameManager::ScenePath(scene.Name)).Success);
        CHECK_SAME_WORLD(authored, Capture());

        std::vector<std::uint8_t> committed;
        std::string error;
        REQUIRE(Serialization::WorldSerializer::ReadFile(GameManager::ScenePath(scene.Name), committed, error));
        CHECK(committed == expected);
        CHECK(Serialization::WorldSerializer::IsTextSave(committed) == scene.PlainText);
    }
}

TEST_CASE("Scene files: every sample scene plays without missing scripts")
{
    for (const auto& scene : SampleScenes::All())
    {
        Fixture::FreshWorld();
        std::string error;
        REQUIRE(GameSceneManager.LoadGame(GameManager::ScenePath(scene.Name), error));
        CHECK_EQ(GameSceneManager.GetActiveScene(), std::string(ScenePlayer::NAME));
        size_t scripted = ECS.Visit<ScriptComponent>().size();
        bool hasSceneScript = !ECS.GetResource<SceneSettings>()->SceneScript.empty();
        TestEnvironment::RunFrames(10, 20.0f);
        CHECK(GameSceneManager.Scripts().MissingScripts().empty());
        CHECK(GameSceneManager.Scripts().InstanceCount() >= scripted + (hasSceneScript ? 1 : 0));
        // Every shape was turned into a mesh by the MeshHandler
        for (Entity e : ECS.Visit<Shape2D>())
            CHECK(ECS.GetComponent<Shape2D>(e).Built);
        for (Entity e : ECS.Visit<Mesh>())
            CHECK(ECS.GetComponent<Mesh>(e).Loaded);
    }
}
