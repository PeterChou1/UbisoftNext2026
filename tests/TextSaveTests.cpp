//---------------------------------------------------------------------------------
// TextSaveTests.cpp
//---------------------------------------------------------------------------------
//
// Plain text saves (SaveFormat::Text): exact round trips, readability, hand
// editing, validation, robustness and the program wide format option
//
#include "AppStub.h"
#include "SampleScenes.h"
#include "SceneEditor.h"
#include "SceneEditorScene.h"
#include "Serialization/TextArchive.h"
#include "WorldFixture.h"

#include <cmath>
#include <filesystem>
#include <limits>

using namespace Serialization;
using Fixture::Capture;
using Fixture::Serializer;

namespace
{
    std::vector<std::uint8_t> Bytes(const std::string& text) { return std::vector<std::uint8_t>(text.begin(), text.end()); }

    void Replace(std::string& text, const std::string& from, const std::string& to)
    {
        std::size_t at = text.find(from);
        REQUIRE(at != std::string::npos);
        text.replace(at, from.size(), to);
    }

    // Restores the binary default whatever a test does
    struct FormatGuard
    {
        ~FormatGuard() { WorldSerializer::SetFileFormat(SaveFormat::Binary); }
    };

    std::string TempPath(const std::string& name)
    {
        return (std::filesystem::temp_directory_path() / ("ubisoft_next_text_" + name)).string();
    }
} // namespace

TEST_CASE("Text saves: every scene feature survives a text round trip, byte for byte")
{
    Fixture::BuildSampleWorld();
    Fixture::WorldImage before = Capture();
    SaveMetadata meta = {{"Scene", "Play"}, {"Name", "sample"}};
    std::vector<std::uint8_t> binary = Serializer().Save(ECS, meta);
    std::string text = Serializer().SaveText(ECS, meta);

    // Text -> binary gives exactly what the binary save wrote
    std::vector<std::string> warnings;
    CHECK(Serializer().TextToBinary(text, warnings) == binary);
    CHECK(warnings.empty());

    Fixture::FreshWorld();
    LoadResult loaded = Serializer().Load(ECS, Bytes(text));
    REQUIRE(loaded.Success);
    CHECK(loaded.Warnings.empty());
    CHECK_EQ(loaded.Metadata["Name"], std::string("sample"));
    CHECK_SAME_WORLD(before, Capture());
}

TEST_CASE("Text saves: every committed scene converts to text and back exactly")
{
    for (const auto& scene : SampleScenes::All())
    {
        std::vector<std::uint8_t> binary;
        std::string error;
        REQUIRE(WorldSerializer::ReadFile(GameManager::ScenePath(scene.Name), binary, error));
        Fixture::FreshWorld();
        REQUIRE(Serializer().Load(ECS, binary).Success);
        std::string text = Serializer().SaveText(ECS, Serializer().Load(ECS, binary).Metadata);
        std::vector<std::string> warnings;
        CHECK(Serializer().TextToBinary(text, warnings) == binary);
        // Much smaller than the binary: the free entity ids are one range
        CHECK(text.size() < binary.size());
    }
}

TEST_CASE("Text saves: the file is readable")
{
    Fixture::BuildSampleWorld();
    std::string text = Serializer().SaveText(ECS, {{"Scene", "Play"}});
    CHECK(text.rfind("UBSV-TEXT 1\n", 0) == 0);
    CHECK(text.find("meta [1] \"Scene\" \"Play\"") != std::string::npos);
    CHECK(text.find("component \"SceneObject\" 1") != std::string::npos);
    CHECK(text.find("\"Player\" \"Player\"") != std::string::npos);
    CHECK(text.find("\"PlayerController\" [1] \"Speed\" 7.5") != std::string::npos);
    CHECK(text.find("resource \"SceneSettings\" 1\n: \"sample\" \"CollectGame\"") != std::string::npos);
    // Free entity ids as a range, not thousands of numbers
    CHECK(text.find("..4999") != std::string::npos);
    CHECK(text.find("\nend\n") != std::string::npos);
    CHECK(WorldSerializer::IsTextSave(Bytes(text)));
    CHECK(!WorldSerializer::IsTextSave(Serializer().Save(ECS)));
}

TEST_CASE("Text saves: hand edits are loaded")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    std::string text = Serializer().SaveText(ECS);
    // Rename the player, change the scene script's lives, add comments / blank lines
    Replace(text, "\"Player\" \"Player\"", "\"Hero\" \"Player\"");
    Replace(text, "\"Lives\" 2", "\"Lives\" 9");
    Replace(text, "end\n", "# the end\n\nend\n");

    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, Bytes(text)).Success);
    CHECK_EQ(SceneObjects::FindByName("Hero"), w.Player);
    CHECK_EQ(ECS.GetResource<SceneSettings>()->SceneParams.at("Lives"), 9.0f);
    // Windows line endings are fine too
    std::string crlf;
    for (char c : Serializer().SaveText(ECS))
        crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
    Fixture::FreshWorld();
    CHECK(Serializer().Load(ECS, Bytes(crlf)).Success);
}

TEST_CASE("Text saves: mistakes are reported with their line and never touch the world")
{
    Fixture::BuildSampleWorld();
    std::string good = Serializer().SaveText(ECS);
    Fixture::WorldImage before = Capture();

    struct Case
    {
        const char* From;
        const char* To;
        const char* Expect;
    };
    const Case cases[] = {
            {"\nend\n", "\n", "no 'end'"},
            {"UBSV-TEXT 1", "UBSV-TEXT 7", "version"},
            {"\"Speed\" 7.5", "\"Speed\" seven", "expected a number"},
            {"\"Speed\" 7.5", "\"Speed\" 7.5 42", "layout mismatch"},
            {"component \"SceneObject\"", "komponent \"SceneObject\"", "unknown keyword"},
            {"\"Player\" \"Player\"", "\"Player \"Player\"", "line"},
            {"\"Player\" \"Player\"", "\"Player\\q\" \"Player\"", "escape"},
            {"component \"Shape2D\" 1 [", "component \"Shape2D\" 1 [9", "records"},
            {"entities 5000", "entities 4000", "MAX_ENTITIES"},
            {"entities 5000 [", "entities 5000 [1", "entit"},
            {"\nend\n", "\nend\nmore\n", "after 'end'"},
    };
    for (const Case& c : cases)
    {
        std::string bad = good;
        Replace(bad, c.From, c.To);
        LoadResult result = Serializer().Load(ECS, Bytes(bad));
        CHECK(!result.Success);
        if (result.Error.find(c.Expect) == std::string::npos)
        {
            ++TestFramework::TotalChecks();
            TestFramework::ReportFailure(__FILE__, __LINE__,
                                         std::string("'") + c.To + "': error '" + result.Error + "' lacks '" + c.Expect + "'");
        }
        CHECK_SAME_WORLD(before, Capture());
    }
    // Errors inside records name the line
    std::string bad = good;
    Replace(bad, "\"Speed\" 7.5", "\"Speed\" x");
    CHECK(Serializer().Load(ECS, Bytes(bad)).Error.find("line ") != std::string::npos);
}

TEST_CASE("Text saves: unknown types are skipped with a warning, newer layouts are refused")
{
    Fixture::SampleWorld w = Fixture::BuildSampleWorld();
    std::string text = Serializer().SaveText(ECS);
    // A component type and a resource from a newer build
    Replace(text, "resource \"SceneSettings\"",
            "component \"Stamina\" 1 [2]\n1: 0.5 true\n2: 0.75 false\nresource \"Weather\" 3\n: \"rain\" 12\nresource \"SceneSettings\"");
    Fixture::FreshWorld();
    LoadResult result = Serializer().Load(ECS, Bytes(text));
    REQUIRE(result.Success);
    REQUIRE(result.Warnings.size() == 2);
    CHECK(result.Warnings[0].find("Stamina") != std::string::npos);
    CHECK(result.Warnings[1].find("Weather") != std::string::npos);
    CHECK(ECS.IsEntityAlive(w.Player));

    std::string newer = Serializer().SaveText(ECS);
    Replace(newer, "component \"Shape2D\" 1", "component \"Shape2D\" 2");
    LoadResult refused = Serializer().Load(ECS, Bytes(newer));
    CHECK(!refused.Success);
    CHECK(refused.Error.find("version 2") != std::string::npos);
}

TEST_CASE("Text saves: strings and special numbers are exact")
{
    Fixture::FreshWorld();
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Tricky \"name\"\\ with\nnew line\tand tab \x01 caf\xc3\xa9",
                                                    Shape2DType::Circle, {0, 0, 0});
    Entity e = SceneObjects::CreateShape(desc);
    const float specials[] = {0.1f,
                              -0.0f,
                              1e-45f,
                              3.4028235e38f,
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity(),
                              1.0f / 3.0f};
    int i = 0;
    std::vector<Entity> holders;
    for (float value : specials)
    {
        Entity holder = SceneObjects::CreateShape(Fixture::ShapeOf("F" + std::to_string(i++), Shape2DType::Circle, {0, 0, 0}));
        ECS.GetComponent<Shape2D>(holder).Width = value;
        holders.push_back(holder);
    }
    Fixture::WorldImage before = Capture();
    std::string text = Serializer().SaveText(ECS);
    CHECK(text.find("\"Tricky \\\"name\\\"\\\\ with\\nnew line\\tand tab \\x01 caf\xc3\xa9\"") != std::string::npos);
    CHECK(text.find(" inf ") != std::string::npos);
    CHECK(text.find(" 0.1 ") != std::string::npos);

    Fixture::FreshWorld();
    REQUIRE(Serializer().Load(ECS, Bytes(text)).Success);
    CHECK_SAME_WORLD(before, Capture());
    CHECK_EQ(ECS.GetComponent<SceneObject>(e).Name, desc.Name);
    CHECK(std::signbit(ECS.GetComponent<Shape2D>(holders[1]).Width));

    // The float formatting is the shortest exact one
    CHECK_EQ(TextOutputArchive::FormatFloat(0.1f), std::string("0.1"));
    CHECK_EQ(TextOutputArchive::FormatFloat(2.5f), std::string("2.5"));
    CHECK_EQ(TextOutputArchive::FormatFloat(0.1), std::string("0.1"));
    CHECK_EQ(TextOutputArchive::FormatFloat(std::nanf("")), std::string("nan"));
}

TEST_CASE("Text saves: random damage never crashes the loader")
{
    Fixture::BuildSampleWorld();
    std::string good = Serializer().SaveText(ECS);
    Fixture::WorldImage before = Capture();
    std::uint32_t seed = 777;
    auto next = [&] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    const char alphabet[] = "0123456789-.[]\":x \n#aeinrt";
    int loaded = 0;
    for (int attempt = 0; attempt < 300; ++attempt)
    {
        std::string bad = good;
        int edits = 1 + next() % 4;
        for (int k = 0; k < edits; ++k)
        {
            std::size_t at = next() % bad.size();
            switch (next() % 3)
            {
            case 0:
                bad.erase(at, 1 + next() % 6);
                break;
            case 1:
                bad.insert(at, 1, alphabet[next() % (sizeof(alphabet) - 1)]);
                break;
            default:
                bad[at] = alphabet[next() % (sizeof(alphabet) - 1)];
            }
        }
        LoadResult result = Serializer().Load(ECS, Bytes(bad));
        if (result.Success)
            ++loaded; // a harmless edit (e.g. in a comment or a value)
        else
            CHECK_SAME_WORLD(before, Capture());
        if (result.Success)
            Serializer().Load(ECS, Bytes(good));
    }
    CHECK(loaded < 300);
}

TEST_CASE("Text saves: the file format option switches every file save")
{
    FormatGuard guard;
    Fixture::BuildSampleWorld();
    Fixture::WorldImage before = Capture();
    std::string path = TempPath("option.ubsave");

    CHECK(WorldSerializer::FileFormat() == SaveFormat::Binary);
    REQUIRE(Serializer().SaveToFile(ECS, path).Success);
    std::vector<std::uint8_t> bytes;
    std::string error;
    REQUIRE(WorldSerializer::ReadFile(path, bytes, error));
    CHECK(!WorldSerializer::IsTextSave(bytes));

    WorldSerializer::SetFileFormat(SaveFormat::Text);
    REQUIRE(Serializer().SaveToFile(ECS, path).Success);
    REQUIRE(WorldSerializer::ReadFile(path, bytes, error));
    CHECK(WorldSerializer::IsTextSave(bytes));
    Fixture::FreshWorld();
    REQUIRE(Serializer().LoadFromFile(ECS, path).Success);
    CHECK_SAME_WORLD(before, Capture());

    // The game's saves and the editor's scene files follow it
    REQUIRE(GameSceneManager.SaveGame(path, error));
    REQUIRE(WorldSerializer::ReadFile(path, bytes, error));
    CHECK(WorldSerializer::IsTextSave(bytes));
    REQUIRE(GameSceneManager.LoadGame(path, error));
    Editor::SceneEditor editor;
    CHECK(WorldSerializer::IsTextSave(editor.SaveSceneToBytes("x")));
    WorldSerializer::SetFileFormat(SaveFormat::Binary);
    CHECK(!WorldSerializer::IsTextSave(editor.SaveSceneToBytes("x")));
    std::filesystem::remove(path);
}

TEST_CASE("Text saves: the editor's Plain text files option")
{
    FormatGuard guard;
    std::string folder = TempPath("editor_scenes");
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    SceneEditorScene& gui = TestEnvironment::Editor();
    gui.SetSceneDirectory(folder);
    AppStub::Reset();
    GameSceneManager.SetActiveScene(TestEnvironment::EDITOR_SCENE);
    AppStub::Get().MouseX = 500;
    AppStub::Get().MouseY = 20;
    TestEnvironment::RunFrame(16);

    auto click = [](float x, float y) {
        AppStub::Get().MouseX = x;
        AppStub::Get().MouseY = y;
        AppStub::Get().LeftDown = true;
        TestEnvironment::RunFrame(16);
        AppStub::Get().LeftDown = false;
        TestEnvironment::RunFrame(16);
    };
    auto find = [](const std::string& text) {
        return AppStub::FindPrinted(text);
    };
    const auto* sceneTab = find("Scene");
    REQUIRE(sceneTab != nullptr);
    click(sceneTab->X + 20, sceneTab->Y + 5);
    const auto* option = find("Plain text files");
    REQUIRE(option != nullptr);
    // The check box sits left of its label
    click(option->X - 18, option->Y + 8);
    CHECK(WorldSerializer::FileFormat() == SaveFormat::Text);
    CHECK(AppStub::WasPrinted("plain text"));

    gui.GetEditor().Place(Editor::ObjectKind::Circle, {1, 0, 1});
    REQUIRE(gui.SaveDocument());
    std::vector<std::uint8_t> bytes;
    std::string error;
    REQUIRE(WorldSerializer::ReadFile(gui.ScenePath(gui.SceneName()), bytes, error));
    CHECK(WorldSerializer::IsTextSave(bytes));
    REQUIRE(gui.OpenScene(gui.SceneName()));
    CHECK_EQ(gui.GetEditor().Objects().size(), size_t(2));
    TestEnvironment::RunFrame(16);
    CHECK(AppStub::WasPrinted("(plain text file)"));

    gui.SetSceneDirectory(GameManager::SCENES_DIRECTORY);
    std::filesystem::remove_all(folder);
}

TEST_CASE("Text saves: enum values outside their range are refused")
{
    Fixture::BuildSampleWorld();
    std::string text = Serializer().SaveText(ECS);
    Fixture::WorldImage before = Capture();
    std::size_t header = text.find("component \"FragShaderTag\" 1 [");
    REQUIRE(header != std::string::npos);
    std::size_t record = text.find('\n', header) + 1;
    std::size_t colon = text.find(':', record);
    std::size_t end = text.find('\n', colon);
    text.replace(colon + 1, end - colon - 1, " 99");
    LoadResult result = Serializer().Load(ECS, Bytes(text));
    CHECK(!result.Success);
    CHECK(result.Error.find("out of range") != std::string::npos);
    CHECK(result.Error.find("line ") != std::string::npos);
    CHECK_SAME_WORLD(before, Capture());
}
