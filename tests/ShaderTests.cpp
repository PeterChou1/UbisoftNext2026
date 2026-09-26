//---------------------------------------------------------------------------------
// ShaderTests.cpp
//---------------------------------------------------------------------------------
//
// Shaders tied to meshes: every shape and model is drawn by the fragment
// shader of its FragShaderTag and the vertex shader of its VertShaderTag.
// The new effect shaders, their names, the renderer picking up a changed
// shader, and the shaders kept by the editor, scene files and prefabs
//
#include "AppStub.h"
#include "AssetServer.h"
#include "Camera.h"
#include "EffectShadersSIMD.h"
#include "EffectVertexShaders.h"
#include "FragShaderTag.h"
#include "GameOptions.h"
#include "RenderConstants.h"
#include "SceneEditor.h"
#include "ShaderLibrary.h"
#include "VertShaderTag.h"
#include "VertexBuffer.h"
#include "WorldFixture.h"
#include "World/Prefab.h"

#include <cmath>
#include <filesystem>
#include <set>
#include <sstream>

using Editor::ObjectKind;
using Editor::SceneEditor;

namespace
{
    bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

    SceneEditor& NewEditor()
    {
        static SceneEditor editor;
        Fixture::FreshWorld();
        editor = SceneEditor{};
        editor.NewScene();
        return editor;
    }

    // Vertex shader ids the renderer uses for the object's vertices
    std::set<std::size_t> VertexShaderIdsOf(Entity e)
    {
        auto constants = ECS.GetResource<RenderConstants>();
        auto buffer = ECS.GetResource<VertexBuffer>();
        std::set<std::size_t> ids;
        BufferRange range = constants->EntityToVertexRange.at(e);
        for (int i = range.first; i < range.second; ++i)
            ids.insert(buffer->Buffer[static_cast<std::size_t>(i)].VertexShaderID);
        return ids;
    }

    std::set<std::size_t> FragmentShaderIdsOf(Entity e)
    {
        auto constants = ECS.GetResource<RenderConstants>();
        auto buffer = ECS.GetResource<VertexBuffer>();
        std::set<std::size_t> ids;
        BufferRange range = constants->EntityToVertexRange.at(e);
        for (int i = range.first; i < range.second; ++i)
            ids.insert(buffer->Buffer[static_cast<std::size_t>(i)].FragShaderID);
        return ids;
    }

    struct SoftwareRenderer
    {
        SoftwareRenderer() { ECS.GetResource<GameOptions>()->LineRendering = false; }
        ~SoftwareRenderer() { ECS.GetResource<GameOptions>()->LineRendering = true; }
    };
} // namespace

TEST_CASE("Shaders: the library names every shader the editor offers")
{
    // Every listed shader has a unique name that finds it again
    std::set<std::string> names;
    for (const auto& shader : ShaderLibrary::FragmentShaders())
    {
        CHECK(names.insert(shader.Name).second);
        FragShaderTypeID found = DefaultFragShaderID;
        REQUIRE(ShaderLibrary::Find(shader.Name, found));
        CHECK(found == shader.Value);
        CHECK_EQ(ShaderLibrary::Name(shader.Value), std::string(shader.Name));
        CHECK(std::string(shader.Description).size() > 0);
    }
    CHECK(names.count("Pulse") == 1);
    CHECK(names.count("Rim") == 1);
    CHECK(names.count("Stripes") == 1);
    // Internal shaders are named but not offered
    CHECK(names.count("Particle") == 0);
    CHECK_EQ(ShaderLibrary::Name(ParticleShaderID), std::string("Particle"));

    CHECK_EQ(ShaderLibrary::VertexShaders().size(), size_t(3));
    VertShaderTypeID vertex = DefaultVertShaderID;
    REQUIRE(ShaderLibrary::Find("Wave", vertex));
    CHECK(vertex == WaveVertShaderID);
    REQUIRE(ShaderLibrary::Find("Sway", vertex));
    CHECK(vertex == SwayVertShaderID);
    CHECK(!ShaderLibrary::Find("Nope", vertex));
    CHECK_EQ(ShaderLibrary::Name(static_cast<VertShaderTypeID>(42)), std::string("#42"));
}

TEST_CASE("Shaders: the asset server creates the new shaders for an entity")
{
    Fixture::FreshWorld();
    AssetServer& server = AssetServer::GetInstance();
    const Entity e = 4000;

    FragShaderTag pulse(PulseShaderID);
    server.SetFragShader(e, pulse);
    CHECK(dynamic_cast<PulseShaderSIMD*>(server.GetFragShader(pulse.FragShaderID).get()) != nullptr);
    FragShaderTag rim(RimShaderID);
    server.SetFragShader(e, rim);
    CHECK(dynamic_cast<RimShaderSIMD*>(server.GetFragShader(rim.FragShaderID).get()) != nullptr);
    FragShaderTag stripes(StripesShaderID);
    server.SetFragShader(e, stripes);
    CHECK(dynamic_cast<StripesShaderSIMD*>(server.GetFragShader(stripes.FragShaderID).get()) != nullptr);
    server.RemoveFragShader(e);

    VertShaderTag wave(WaveVertShaderID);
    server.SetVertShader(e, wave);
    CHECK(dynamic_cast<WaveVertexShader*>(server.GetVertShader(wave.VertShaderID).get()) != nullptr);
    VertShaderTag sway(SwayVertShaderID);
    server.SetVertShader(e, sway);
    CHECK(dynamic_cast<SwayVertexShader*>(server.GetVertShader(sway.VertShaderID).get()) != nullptr);
    server.RemoveVertShader(e);
}

TEST_CASE("Shaders: vertex shaders move the drawn vertex, not the stored one")
{
    Vertex v;
    v.LocalPosition = Vec3(0.0f, 2.0f, 0.0f);
    v.Position = Vec3(1.0f, 2.0f, 3.0f);

    // Wave: only the height changes, by at most the amplitude, over time
    Vec3 w0 = WaveVertexShader::Displace(v, 0.0f);
    Vec3 w1 = WaveVertexShader::Displace(v, 0.5f);
    CHECK_EQ(w0.X, 1.0f);
    CHECK_EQ(w0.Z, 3.0f);
    CHECK(std::fabs(w0.Y - 2.0f) <= WaveVertexShader::AMPLITUDE + 1e-5f);
    CHECK(!Near(w0.Y, w1.Y));
    float expected = 2.0f + WaveVertexShader::AMPLITUDE * std::sin((1.0f + 3.0f) * WaveVertexShader::FREQUENCY);
    CHECK(Near(w0.Y, expected));

    // Sway: sideways, more at the top, nothing at the base
    Vec3 top = SwayVertexShader::Displace(v, 0.3f);
    CHECK_EQ(top.Y, 2.0f);
    CHECK(!Near(top.X, 1.0f));
    Vertex base = v;
    base.LocalPosition.Y = 0.0f;
    Vec3 foot = SwayVertexShader::Displace(base, 0.3f);
    CHECK(Near(foot.X, 1.0f));
    CHECK(Near(foot.Z, 3.0f));

    // Shading projects the displaced point and leaves the buffer's world
    // position alone (it would drift a little more every frame)
    Fixture::FreshWorld();
    auto camera = ECS.GetResource<Camera>();
    SceneObjects::ApplyCamera(*camera, {0, 0, 0}, 20.0f);
    DirectionalLight light;
    WaveVertexShader wave;
    wave.DeltaTime = 0.5f;
    Vertex shaded = v;
    wave.Shade(shaded, *camera, light);
    CHECK_EQ(shaded.Position.Y, 2.0f);
    Vec3 cam = camera->WorldToCamera(WaveVertexShader::Displace(v, 0.5f));
    CHECK(Near(shaded.PositionCamera.X, cam.X));
    CHECK(Near(shaded.PositionCamera.Y, cam.Y));
    CHECK(Near(shaded.PositionCamera.Z, cam.Z));
}

TEST_CASE("Shaders: changing an object's shaders reaches the renderer's vertices")
{
    Fixture::FreshWorld();
    Entity shape = SceneObjects::CreateShape(Fixture::ShapeOf("Shape", Shape2DType::Rectangle, {0, 0, 0}));
    Entity model = SceneObjects::CreateModel("Crate", "Box", {3, 0, 0});
    TestEnvironment::RunFrame(16.0f);
    CHECK(VertexShaderIdsOf(shape) == std::set<std::size_t>{0});

    for (Entity e : {shape, model})
    {
        // A vertex shader added after the mesh was built
        SceneObjects::SetVertexShader(e, WaveVertShaderID);
        TestEnvironment::RunFrame(16.0f);
        const VertShaderTag& tag = ECS.GetComponent<VertShaderTag>(e);
        REQUIRE(tag.Initialized);
        CHECK(tag.VertShaderID != 0);
        CHECK(VertexShaderIdsOf(e) == std::set<std::size_t>{tag.VertShaderID});
        CHECK(dynamic_cast<WaveVertexShader*>(AssetServer::GetInstance().GetVertShader(tag.VertShaderID).get()) !=
              nullptr);
        // Changed again: the vertices follow (models used to get the vertex
        // shader's id written into their fragment shader id)
        SceneObjects::SetVertexShader(e, SwayVertShaderID);
        TestEnvironment::RunFrame(16.0f);
        const VertShaderTag& sway = ECS.GetComponent<VertShaderTag>(e);
        CHECK(VertexShaderIdsOf(e) == std::set<std::size_t>{sway.VertShaderID});
        CHECK(dynamic_cast<SwayVertexShader*>(AssetServer::GetInstance().GetVertShader(sway.VertShaderID).get()) !=
              nullptr);

        SceneObjects::SetFragmentShader(e, RimShaderID);
        TestEnvironment::RunFrame(16.0f);
        const FragShaderTag& rim = ECS.GetComponent<FragShaderTag>(e);
        CHECK(FragmentShaderIdsOf(e) == std::set<std::size_t>{rim.FragShaderID});
        CHECK(dynamic_cast<RimShaderSIMD*>(AssetServer::GetInstance().GetFragShader(rim.FragShaderID).get()) !=
              nullptr);
        CHECK(SceneObjects::FragmentShaderOf(e) == RimShaderID);
        CHECK(SceneObjects::VertexShaderOf(e) == SwayVertShaderID);
    }
    // No tag: the defaults, and the default vertex shader adds no tag
    Entity plain = SceneObjects::CreateEmpty("Plain", {0, 0, 0});
    CHECK(SceneObjects::VertexShaderOf(plain) == DefaultVertShaderID);
    SceneObjects::SetVertexShader(plain, DefaultVertShaderID);
    CHECK(!ECS.HasComponent<VertShaderTag>(plain));
}

TEST_CASE("Shaders: the effect shaders draw the object in its colour")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    SoftwareRenderer software;
    auto camera = ECS.GetResource<Camera>();
    SceneObjects::ApplyCamera(*camera, {0, 0, 0}, 20.0f);
    SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Red", Shape2DType::Rectangle, {0, 0, 0});
    desc.Shape.Width = 6.0f;
    desc.Shape.Height = 6.0f;
    desc.Shape.Color = {0.9f, 0.1f, 0.1f};
    Entity red = SceneObjects::CreateShape(desc);
    Vec2 center = camera->WorldPointToScreenSpace({0, 0.25f, 0});
    for (FragShaderTypeID shader : {PulseShaderID, RimShaderID, StripesShaderID})
    {
        SceneObjects::SetFragmentShader(red, shader);
        TestEnvironment::RunFrame(16.0f);
        TestEnvironment::RunFrame(16.0f);
        AppStub::Pixel p = AppStub::PixelAt(static_cast<int>(center.X), static_cast<int>(center.Y));
        CHECK(p.R > 0.05f);
        CHECK(p.R > 2.0f * p.B);
    }
}

TEST_CASE("Shaders: the editor sets them with undo, duplicates and scene files keep them")
{
    SceneEditor& editor = NewEditor();
    Entity e = editor.Place(ObjectKind::Circle, {2, 0, 2});
    Entity empty = editor.Place(ObjectKind::Empty, {-2, 0, 2});
    CHECK(SceneObjects::FragmentShaderOf(e) == ShapeShaderID);

    std::size_t undo = editor.UndoCount();
    REQUIRE(editor.SetFragmentShader(e, PulseShaderID));
    REQUIRE(editor.SetVertexShader(e, WaveVertShaderID));
    CHECK_EQ(editor.UndoCount(), undo + 2);
    CHECK(SceneObjects::FragmentShaderOf(e) == PulseShaderID);
    CHECK(SceneObjects::VertexShaderOf(e) == WaveVertShaderID);
    // Same value: no undo step; empties have nothing to shade; bad ids refused
    REQUIRE(editor.SetVertexShader(e, WaveVertShaderID));
    CHECK_EQ(editor.UndoCount(), undo + 2);
    CHECK(!editor.SetFragmentShader(empty, RimShaderID));
    CHECK(!editor.SetVertexShader(e, static_cast<VertShaderTypeID>(99)));

    // Undo / redo restore the world: find the object again by name
    std::string name = editor.NameOf(e);
    REQUIRE(editor.Undo());
    CHECK(SceneObjects::VertexShaderOf(SceneObjects::FindByName(name)) == DefaultVertShaderID);
    REQUIRE(editor.Redo());
    e = SceneObjects::FindByName(name);
    CHECK(SceneObjects::VertexShaderOf(e) == WaveVertShaderID);

    // A duplicate is drawn the same way
    Entity copy = editor.Duplicate(e);
    REQUIRE(copy != NULL_ENTITY);
    CHECK(SceneObjects::FragmentShaderOf(copy) == PulseShaderID);
    CHECK(SceneObjects::VertexShaderOf(copy) == WaveVertShaderID);

    // Saved and loaded with the scene
    std::string path = (std::filesystem::temp_directory_path() / "shader_scene.ubsave").string();
    REQUIRE(editor.SaveScene(path, "shaders").Success);
    editor.NewScene();
    REQUIRE(editor.LoadScene(path).Success);
    Entity loaded = SceneObjects::FindByName(editor.NameOf(copy));
    REQUIRE(loaded != NULL_ENTITY);
    CHECK(SceneObjects::FragmentShaderOf(loaded) == PulseShaderID);
    CHECK(SceneObjects::VertexShaderOf(loaded) == WaveVertShaderID);
    std::filesystem::remove(path);
}

TEST_CASE("Shaders: prefabs keep their objects' shaders; version 1 prefabs get the defaults")
{
    SceneEditor& editor = NewEditor();
    Entity root = editor.AddEmpty({0, 0, 0});
    editor.Rename(root, "Lamp");
    Entity post = editor.Create(ObjectKind::Rectangle, {0, 0, 0}, root);
    editor.SetFragmentShader(post, StripesShaderID);
    editor.SetVertexShader(post, SwayVertShaderID);
    Editor::PlaceSettings ball;
    ball.Model = "GolfBall";
    Entity bulb = editor.Create(ObjectKind::Model, {0, 0, 1}, root, ball);
    editor.SetFragmentShader(bulb, RimShaderID);

    Prefab::Data prefab = Prefab::Capture(root, "lamp");
    REQUIRE(prefab.Objects.size() == 3u);
    CHECK(prefab.Objects[1].FragShader == StripesShaderID);
    CHECK(prefab.Objects[1].VertShader == SwayVertShaderID);
    CHECK(prefab.Objects[2].FragShader == RimShaderID);

    for (auto format : {Serialization::SaveFormat::Binary, Serialization::SaveFormat::Text})
    {
        Prefab::Data loaded;
        std::string error;
        REQUIRE(Prefab::Load(Prefab::Save(prefab, format), loaded, error));
        REQUIRE(loaded.Objects.size() == 3u);
        CHECK(loaded.Objects[1].FragShader == StripesShaderID);
        CHECK(loaded.Objects[1].VertShader == SwayVertShaderID);
        CHECK(loaded.Objects[2].FragShader == RimShaderID);
        Entity copy = Prefab::Instantiate(loaded, {5, 0, 5});
        std::vector<Entity> objects = Prefab::InstanceObjects(copy);
        REQUIRE(objects.size() == 3u);
        CHECK(SceneObjects::FragmentShaderOf(objects[1]) == StripesShaderID);
        CHECK(SceneObjects::VertexShaderOf(objects[1]) == SwayVertShaderID);
        CHECK(SceneObjects::FragmentShaderOf(objects[2]) == RimShaderID);
    }

    // A version 1 text prefab (no shader fields): shapes get the shape shader,
    // models the lit one
    std::vector<std::uint8_t> bytes = Prefab::Save(prefab, Serialization::SaveFormat::Text);
    std::string text(bytes.begin(), bytes.end());
    std::string old;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        if (line.rfind("UBPF-TEXT", 0) == 0)
            line = "UBPF-TEXT 1";
        else if (line.rfind("object ", 0) == 0)
        {
            // Drop the two shader values at the end of the record
            line = line.substr(0, line.rfind(' '));
            line = line.substr(0, line.rfind(' '));
        }
        old += line + "\n";
    }
    Prefab::Data v1;
    std::string error;
    REQUIRE(Prefab::Load(std::vector<std::uint8_t>(old.begin(), old.end()), v1, error));
    REQUIRE(v1.Objects.size() == 3u);
    CHECK(v1.Objects[1].FragShader == ShapeShaderID);
    CHECK(v1.Objects[1].VertShader == DefaultVertShaderID);
    CHECK(v1.Objects[2].FragShader == BlinnPhongID);
}
