//---------------------------------------------------------------------------------
// ModelImportTests.cpp
//---------------------------------------------------------------------------------
//
// Importing custom .obj models (Engine/ModelImport) and the OBJ reader it
// relies on (Utils::LoadInstance), plus the editor's IMPORT .OBJ box
//
#include "AppStub.h"
#include "AssetServer.h"
#include "Mesh.h"
#include "ModelImport.h"
#include "SceneEditorScene.h"
#include "Utils.h"
#include "WorldFixture.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace
{
    // A scratch source folder and models folder, removed afterwards
    struct Folders
    {
        fs::path Source;
        fs::path Models;
        explicit Folders(const std::string& name)
        {
            fs::path root = fs::temp_directory_path() / ("ubisoft_next_import_" + name);
            fs::remove_all(root);
            Source = root / "art";
            Models = root / "models";
            fs::create_directories(Source);
        }
        ~Folders() { fs::remove_all(Source.parent_path()); }
        fs::path Write(const std::string& file, const std::string& text) const
        {
            std::ofstream(Source / file) << text;
            return Source / file;
        }
        std::string ModelsDir() const { return Models.string() + "/"; }
    };

    std::string ReadAll(const fs::path& path)
    {
        std::ifstream in(path);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    // A unit cube made of quads, with a material library
    const char* CUBE_OBJ = "mtllib cube.mtl\n"
                           "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 1\nv 1 0 1\nv 1 1 1\nv 0 1 1\n"
                           "usemtl Red\n"
                           "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n";
    const char* CUBE_MTL = "newmtl Red\nKa 1 1 1\nKd 0.9 0.1 0.1\nKs 0 0 0\nNs 10\n";

    MeshInstance Load(const fs::path& path, std::vector<Material>& materials)
    {
        MeshInstance mesh;
        REQUIRE(Utils::LoadInstance(path.string(), mesh, materials));
        return mesh;
    }
} // namespace

TEST_CASE("OBJ reader: polygons, index forms, negative indices and bad data")
{
    Folders folders("reader");
    std::vector<Material> materials;

    // Quads and a pentagon are triangulated (fan)
    MeshInstance cube = Load(folders.Write("cube.obj", std::string(CUBE_OBJ) + "f 1 2 3 7 5\n"), materials);
    CHECK_EQ(cube.indices.size() / 3, size_t(6 * 2 + 3));

    // v/vt/vn, v//vn and negative (relative) indices
    MeshInstance forms = Load(folders.Write("forms.obj",
                                            "v 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nvn 0 0 1\n"
                                            "f 1/1/1 2/2/1 3/3/1\n"
                                            "f 1//1 2//1 3//1\n"
                                            "f -3 -2 -1\n"),
                              materials);
    CHECK_EQ(forms.indices.size(), size_t(9));
    for (const Vertex& v : forms.vertices)
        CHECK(std::fabs(v.LocalNormal.Z) > 0.99f);

    // Out of range / zero indices and junk faces are skipped, not read out of bounds
    MeshInstance broken = Load(folders.Write("broken.obj",
                                             "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
                                             "f 1 2 3\n"
                                             "f 1 2 99\nf 0 1 2\nf 1 2\nf a b c\nf 1/9/1 2 3\n"
                                             "o something\ns off\ng group\n"),
                               materials);
    CHECK_EQ(broken.indices.size(), size_t(3));

    // A missing material library or material falls back to the default material
    MeshInstance nomtl = Load(folders.Write("nomtl.obj", "mtllib missing.mtl\nusemtl Nothing\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"),
                              materials);
    REQUIRE(nomtl.vertices.size() == 3);
    CHECK_EQ(nomtl.vertices[0].TextureID, -1);

    // Materials from the library are applied
    folders.Write("cube.mtl", CUBE_MTL);
    std::vector<Material> cubeMaterials;
    MeshInstance coloured = Load(folders.Source / "cube.obj", cubeMaterials);
    REQUIRE(cubeMaterials.size() == 1);
    CHECK(cubeMaterials[0].diffuse.X > 0.8f);
    CHECK_EQ(coloured.vertices[0].TextureID, 0);
}

TEST_CASE("Model import: copies the .obj and its materials, the model is usable by name")
{
    Folders folders("copy");
    fs::path obj = folders.Write("Big Crate!.obj", CUBE_OBJ);
    folders.Write("cube.mtl", CUBE_MTL);

    ModelImport::Result result = ModelImport::Import(obj.string(), folders.ModelsDir());
    REQUIRE(result.Success);
    CHECK_EQ(result.Name, std::string("Big_Crate_"));
    CHECK_EQ(result.Triangles, size_t(12));
    CHECK(!result.AlreadyImported);
    CHECK(result.Warnings.empty());
    CHECK(fs::exists(folders.Models / "Big_Crate_.obj"));
    CHECK(fs::exists(folders.Models / "cube.mtl"));
    std::vector<Material> materials;
    MeshInstance copy = Load(folders.Models / "Big_Crate_.obj", materials);
    CHECK_EQ(copy.indices.size(), size_t(36));
    CHECK_EQ(materials.size(), size_t(1));

    // The same file again: nothing new is copied
    ModelImport::Result again = ModelImport::Import(obj.string(), folders.ModelsDir());
    REQUIRE(again.Success);
    CHECK(again.AlreadyImported);
    CHECK_EQ(again.Name, result.Name);

    // A different model with the same name gets a numbered name, and its
    // different material library with the same file name is renamed
    fs::path other = folders.Source / "second";
    fs::create_directories(other);
    std::ofstream(other / "Big Crate!.obj") << "mtllib cube.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl Red\nf 1 2 3\n";
    std::ofstream(other / "cube.mtl") << "newmtl Red\nKd 0 0 1\n";
    ModelImport::Result second = ModelImport::Import((other / "Big Crate!.obj").string(), folders.ModelsDir());
    REQUIRE(second.Success);
    CHECK_EQ(second.Name, std::string("Big_Crate__2"));
    CHECK(fs::exists(folders.Models / "Big_Crate__cube.mtl"));
    CHECK(ReadAll(folders.Models / "Big_Crate__2.obj").find("mtllib Big_Crate__cube.mtl") != std::string::npos);
    CHECK(ReadAll(folders.Models / "cube.mtl").find("0.9 0.1 0.1") != std::string::npos);
}

TEST_CASE("Model import: problems are reported, nothing is copied")
{
    Folders folders("errors");
    auto failed = [&](const std::string& path, const std::string& expect) {
        ModelImport::Result result = ModelImport::Import(path, folders.ModelsDir());
        CHECK(!result.Success);
        CHECK(result.Error.find(expect) != std::string::npos);
    };
    failed("", "No file");
    failed((folders.Source / "nope.obj").string(), "not found");
    failed(folders.Write("model.fbx", "binary").string(), "Not an .obj");
    failed(folders.Write("empty.obj", "# only a comment\nv 0 0 0\n").string(), "no faces");
    CHECK(!fs::exists(folders.Models) || fs::is_empty(folders.Models));

    // Quoted paths (from a file browser) work; a missing .mtl is only a warning
    fs::path obj = folders.Write("tri.obj", "mtllib gone.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");
    ModelImport::Result quoted = ModelImport::Import("  \"" + obj.string() + "\" ", folders.ModelsDir());
    REQUIRE(quoted.Success);
    REQUIRE(quoted.Warnings.size() == 1);
    CHECK(quoted.Warnings[0].find("gone.mtl") != std::string::npos);
}

TEST_CASE("Model import: the editor's import box (Assets), then place the model")
{
    // A file dropped in data/import, imported by typing its name
    const std::string name = "zz_test_import_cube";
    fs::path dropped = fs::path(SceneEditorScene::IMPORT_DIRECTORY) / (name + ".obj");
    fs::path model = fs::path(AssetServer::MODEL_DIRECTORY) / (name + ".obj");
    fs::create_directories(SceneEditorScene::IMPORT_DIRECTORY);
    std::ofstream(dropped) << "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 1\nv 1 0 1\nv 1 1 1\nv 0 1 1\n"
                              "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n";

    AppStub::Reset();
    GameSceneManager.SetActiveScene(TestEnvironment::EDITOR_SCENE);
    AppStub::Get().MouseX = 500;
    AppStub::Get().MouseY = 20;
    TestEnvironment::RunFrame(16);
    SceneEditorScene& gui = TestEnvironment::Editor();

    // The empty box shows "import .obj"
    const auto* label = AppStub::FindPrinted("import .obj");
    REQUIRE(label != nullptr);
    AppStub::Get().MouseX = label->X + 20.0f;
    AppStub::Get().MouseY = label->Y + 4.0f;
    AppStub::Get().LeftDown = true;
    TestEnvironment::RunFrame(16);
    AppStub::Get().LeftDown = false;
    AppStub::Type(name + "\r");
    TestEnvironment::RunFrame(16);
    TestEnvironment::RunFrame(16);
    CHECK(fs::exists(model));
    // Ready to place: it is listed in Assets and the next click places it
    CHECK_EQ(gui.PlacingAsset(), name);
    CHECK(AppStub::WasPrinted("Imported " + name));

    // Placed like any model, rendered by the MeshHandler
    Editor::PlaceSettings settings;
    settings.Model = gui.PlacingAsset();
    Entity e = gui.GetEditor().Create(Editor::ObjectKind::Model, {2, 0, 2}, NULL_ENTITY, settings);
    gui.StartPlacingModel("");
    TestEnvironment::RunFrame(16);
    REQUIRE(e != NULL_ENTITY);
    CHECK_EQ(ECS.GetComponent<Mesh>(e).Model, name);
    CHECK(ECS.GetComponent<Mesh>(e).Loaded);
    CHECK_EQ(AssetServer::GetInstance().GetModel(name).indices.size(), size_t(36));

    // Errors show in the status bar
    CHECK(!gui.Import("does_not_exist_anywhere"));
    TestEnvironment::RunFrame(16);
    CHECK(AppStub::WasPrinted("Import failed"));

    // The example model shipped in data/import
    fs::path pyramid = fs::path(AssetServer::MODEL_DIRECTORY) / "pyramid.obj";
    bool pyramidExisted = fs::exists(pyramid);
    CHECK(gui.Import("pyramid"));
    CHECK_EQ(AssetServer::GetInstance().GetModel("pyramid").indices.size() / 3, size_t(6));

    gui.GetEditor().NewScene();
    fs::remove(dropped);
    fs::remove(model);
    if (!pyramidExisted)
    {
        fs::remove(pyramid);
        fs::remove(fs::path(AssetServer::MODEL_DIRECTORY) / "pyramid.mtl");
    }
    AssetServer::GetInstance().ForgetModel(name);
}
