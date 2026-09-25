//---------------------------------------------------------------------------------
// LightTests.cpp
//---------------------------------------------------------------------------------
//
// The scene's light is an object (Transform + SceneLight, SceneLight.h),
// applied to the renderer every frame. Also: fragment shaders show when a
// scene file is played (software rasterizer by default, and at the triangle
// corners with the hardware triangles)
//
#include "AppStub.h"
#include "Camera.h"
#include "GameOptions.h"
#include "Lighting.h"
#include "SceneEditor.h"
#include "WorldFixture.h"
#include "World/SceneCamera.h"
#include "World/SceneLight.h"

#include <cmath>
#include <filesystem>

using Editor::ObjectKind;
using Editor::SceneEditor;

namespace
{
    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    bool Near(const Vec3& a, const Vec3& b, float eps = 1e-3f)
    {
        return Near(a.X, b.X, eps) && Near(a.Y, b.Y, eps) && Near(a.Z, b.Z, eps);
    }

    SceneEditor& NewEditor()
    {
        static SceneEditor editor;
        Fixture::FreshWorld();
        editor = SceneEditor{};
        editor.NewScene();
        return editor;
    }

    struct SoftwareRenderer
    {
        SoftwareRenderer() { ECS.GetResource<GameOptions>()->LineRendering = false; }
        ~SoftwareRenderer() { ECS.GetResource<GameOptions>()->LineRendering = true; }
    };

    AppStub::Pixel PixelOf(const Vec3& world)
    {
        Vec2 s = ECS.GetResource<Camera>()->WorldPointToScreenSpace(world);
        return AppStub::PixelAt(static_cast<int>(s.X), static_cast<int>(s.Y));
    }

    // A white square at the origin, the scene's light and camera objects
    Entity LitSquare()
    {
        Fixture::FreshWorld();
        AppStub::Reset();
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf("Square", Shape2DType::Rectangle, {0, 0, 0});
        desc.Shape.Width = 6.0f;
        desc.Shape.Height = 6.0f;
        desc.Shape.Color = {0.9f, 0.9f, 0.9f};
        Entity square = SceneObjects::CreateShape(desc);
        SceneLighting::Create();
        SceneCamera::Create(SceneCamera::View{});
        TestEnvironment::Player().OnWorldRestored();
        return square;
    }
} // namespace

TEST_CASE("Light: the default light is where the old fixed light was")
{
    SceneLighting::Settings settings;
    CHECK(Near(settings.Position, Vec3(0, 25, -5)));
    // It shines at the centre of the field
    CHECK(Near(SceneLighting::GroundTarget(settings), Vec3(0, 0, 0), 0.01f));
    // Straight down, and level
    CHECK(Near(SceneLighting::Direction(0.0f, 89.0f).Y, -0.9998f, 1e-3f));
    CHECK(Near(SceneLighting::Direction(90.0f, 0.0f), Vec3(1, 0, 0)));
    // A light pointing up never reaches the ground: a point along its line
    settings.Light.Pitch = -10.0f;
    CHECK(SceneLighting::GroundTarget(settings).Y > settings.Position.Y);

    // No light object: the defaults light the scene, with shadows
    Fixture::FreshWorld();
    CHECK_EQ(SceneLighting::Find(), NULL_ENTITY);
    TestEnvironment::RunFrame(16.0f);
    DirectionalLight& light = ECS.GetResource<Lighting>()->GetDirectionalLight();
    CHECK(Near(light.Position, Vec3(0, 25, -5)));
    CHECK_EQ(light.Ambient, 0.45f);
    CHECK(ECS.GetResource<GameOptions>()->LightShadows);
}

TEST_CASE("Light: the light object drives the renderer's light every frame")
{
    Fixture::FreshWorld();
    SceneLighting::Settings settings;
    settings.Position = {4, 12, 2};
    settings.Yaw = 90.0f;
    settings.Light.Pitch = 45.0f;
    settings.Light.Color = {1.0f, 0.5f, 0.25f};
    settings.Light.Intensity = 2.0f;
    settings.Light.Ambient = 0.2f;
    settings.Light.Shadows = false;
    Entity e = SceneLighting::Create(settings, "Sun");
    CHECK_EQ(SceneLighting::Find(), e);
    CHECK_EQ(ECS.GetComponent<SceneObject>(e).Tag, std::string("Light"));
    TestEnvironment::RunFrame(16.0f);

    DirectionalLight& light = ECS.GetResource<Lighting>()->GetDirectionalLight();
    CHECK(Near(light.Position, Vec3(4, 12, 2)));
    CHECK(Near(light.Color, Vec3(1.0f, 0.5f, 0.25f)));
    CHECK_EQ(light.Intensity, 2.0f);
    CHECK_EQ(light.Ambient, 0.2f);
    auto options = ECS.GetResource<GameOptions>();
    CHECK(!options->LightShadows);
    CHECK(!options->ShadowsOn());
    // Shines along +X, 45 degrees down: reaches the ground 12 units away
    CHECK(Near(SceneLighting::GroundTarget(SceneLighting::Current()), Vec3(16, 0, 2), 0.01f));

    // Moving / turning the object moves the light (scripts can do it too)
    SceneObjects::SetPosition(e, {0, 20, 0});
    ECS.GetComponent<SceneLight>(e).Shadows = true;
    TestEnvironment::RunFrame(16.0f);
    CHECK(Near(light.Position, Vec3(0, 20, 0)));
    CHECK(options->LightShadows);
    // Shadows need the software rasterizer and the quality setting
    options->LineRendering = false;
    CHECK(options->ShadowsOn());
    options->ShadowMapping = false;
    CHECK(!options->ShadowsOn());
    options->ShadowMapping = true;
    options->LineRendering = true;
    CHECK(!options->ShadowsOn());
}

TEST_CASE("Light: its colour, intensity and ambient change the rendered frame")
{
    Entity square = LitSquare();
    Entity light = SceneLighting::Find();
    SoftwareRenderer software;
    TestEnvironment::RunFrames(2, 16.0f);
    AppStub::Pixel white = PixelOf({0, 0.25f, 0});
    CHECK(white.R > 0.3f);
    CHECK(Near(white.R, white.B, 0.02f));

    // A red light
    ECS.GetComponent<SceneLight>(light).Color = {1.0f, 0.2f, 0.2f};
    TestEnvironment::RunFrames(2, 16.0f);
    AppStub::Pixel red = PixelOf({0, 0.25f, 0});
    CHECK(red.R > 2.0f * red.B);

    // No direct light: only the ambient part is left
    ECS.GetComponent<SceneLight>(light).Color = {1.0f, 1.0f, 1.0f};
    ECS.GetComponent<SceneLight>(light).Intensity = 0.0f;
    TestEnvironment::RunFrames(2, 16.0f);
    AppStub::Pixel dim = PixelOf({0, 0.25f, 0});
    CHECK(dim.R < white.R - 0.1f);
    CHECK(Near(dim.R, 0.9f * 0.45f, 0.03f));
    ECS.GetComponent<SceneLight>(light).Ambient = 0.0f;
    TestEnvironment::RunFrames(2, 16.0f);
    CHECK(PixelOf({0, 0.25f, 0}).R < 0.02f);
    (void)square;
}

TEST_CASE("Light: the Shadows switch of the light turns the shadow map off")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    SceneObjects::ShapeDesc ground = Fixture::ShapeOf("Ground", Shape2DType::Rectangle, {0, -0.1f, 0});
    ground.Shape.Width = 30.0f;
    ground.Shape.Height = 30.0f;
    ground.Shape.Thickness = 0.1f;
    ground.Shape.Color = {0.8f, 0.8f, 0.8f};
    SceneObjects::CreateShape(ground);
    SceneObjects::ShapeDesc pillar = Fixture::ShapeOf("Pillar", Shape2DType::Rectangle, {0, 0, 0});
    pillar.Shape.Width = 2.0f;
    pillar.Shape.Height = 2.0f;
    pillar.Shape.Thickness = 8.0f;
    SceneObjects::CreateShape(pillar);
    Entity light = SceneLighting::Create();
    SceneCamera::View view;
    view.Yaw = 180.0f;
    SceneCamera::Create(view);
    TestEnvironment::Player().OnWorldRestored();
    SoftwareRenderer software;

    auto brightness = [](const Vec3& p) {
        AppStub::Pixel pixel = PixelOf(p);
        return pixel.R + pixel.G + pixel.B;
    };
    TestEnvironment::RunFrames(2, 16.0f);
    CHECK(brightness({0, 0, 2.8f}) < 0.8f * brightness({6, 0, 2.8f}));
    ECS.GetComponent<SceneLight>(light).Shadows = false;
    TestEnvironment::RunFrames(2, 16.0f);
    CHECK(brightness({0, 0, 2.8f}) > 0.8f * brightness({6, 0, 2.8f}));

    // Moving the light moves the shadow: from +Z the shadow falls to -Z
    ECS.GetComponent<SceneLight>(light).Shadows = true;
    SceneObjects::SetPosition(light, {0, 25, 5});
    SceneObjects::SetYaw(light, 180.0f);
    TestEnvironment::RunFrames(2, 16.0f);
    CHECK(brightness({0, 0, 2.8f}) > 0.8f * brightness({6, 0, 2.8f}));
}

TEST_CASE("Light: the editor's light object")
{
    SceneEditor& editor = NewEditor();
    Entity light = editor.LightObject();
    REQUIRE(light != NULL_ENTITY);
    CHECK(SceneObjects::IsEmpty(light));

    // Aim it: one undo step, yaw and pitch towards the point
    std::size_t undo = editor.UndoCount();
    REQUIRE(editor.AimLight(light, {10, 0, -5}));
    CHECK_EQ(editor.UndoCount(), undo + 1);
    CHECK(Near(SceneObjects::GetYaw(light), 90.0f, 0.01f));
    CHECK(Near(ECS.GetComponent<SceneLight>(light).Pitch, std::atan2(25.0f, 10.0f) * 57.29578f, 0.01f));
    CHECK(Near(SceneLighting::GroundTarget(SceneLighting::Current()), Vec3(10, 0, -5), 0.01f));
    CHECK(!editor.AimLight(editor.Objects()[0], {0, 0, 0}));
    REQUIRE(editor.Undo());
    CHECK(Near(SceneLighting::GroundTarget(SceneLighting::Current()), Vec3(0, 0, 0), 0.01f));

    // Raised / lowered like any object; its fields like any component
    light = editor.LightObject();
    REQUIRE(editor.SetHeight(light, 30.0f));
    CHECK_EQ(SceneObjects::GetPosition(light).Y, 30.0f);
    REQUIRE(editor.SetField(light, "SceneLight", "Intensity", 1.5));
    CHECK_EQ(ECS.GetComponent<SceneLight>(light).Intensity, 1.5f);
    REQUIRE(editor.SetField(light, "SceneLight", "Intensity", 9.0));
    CHECK_EQ(ECS.GetComponent<SceneLight>(light).Intensity, 3.0f);

    // AddLight: above the point, shining down; the first light stays the light
    Entity extra = editor.AddLight({4, 0, 4});
    CHECK_EQ(editor.Selected(), extra);
    CHECK(Near(SceneObjects::GetPosition(extra), Vec3(4, 25, 4)));
    CHECK_EQ(editor.NameOf(extra), std::string("Light"));
    CHECK_EQ(editor.LightObject(), light);

    // Picking: the ground under the light picks the object there, a ray
    // through the light's marker picks the light
    Entity box = editor.Place(ObjectKind::Rectangle, {4, 0, 4});
    CHECK_EQ(editor.Pick({4, 0, 4}), box);
    auto throughLight = [](float height) { return Vec3(4, height, 4); };
    float hit = 0.0f;
    CHECK_EQ(editor.PickRay(throughLight, &hit), extra);
    CHECK_EQ(hit, 25.0f);

    // Kept by scene files; not taken into prefabs
    std::string path = (std::filesystem::temp_directory_path() / "light_scene.ubsave").string();
    REQUIRE(editor.SaveScene(path, "light").Success);
    editor.NewScene();
    REQUIRE(editor.LoadScene(path).Success);
    REQUIRE(editor.LightObject() != NULL_ENTITY);
    CHECK_EQ(SceneObjects::GetPosition(editor.LightObject()).Y, 30.0f);
    CHECK_EQ(ECS.GetComponent<SceneLight>(editor.LightObject()).Intensity, 3.0f);
    std::filesystem::remove(path);
    Prefab::Data stage = editor.CaptureStage("all");
    for (const Prefab::Object& o : stage.Objects)
        CHECK(o.Tag != std::string("Light"));
}

TEST_CASE("Render: a played scene file shows its fragment shaders")
{
    // Authored in the editor, saved, then played like the Game does
    SceneEditor& editor = NewEditor();
    Editor::PlaceSettings red;
    red.Width = 6.0f;
    red.Height = 6.0f;
    red.Color = {0.9f, 0.1f, 0.1f};
    Entity e = editor.Place(ObjectKind::Rectangle, {0, 0, 0}, red);
    editor.SetFragmentShader(e, PulseShaderID);
    std::string path = (std::filesystem::temp_directory_path() / "pulse_scene.ubsave").string();
    REQUIRE(editor.SaveScene(path, "pulse").Success);

    // The Game plays with the software rasterizer (the default)
    CHECK(!GameOptions{}.LineRendering);
    CHECK(GameOptions{}.ShadowMapping);
    AppStub::Reset();
    SoftwareRenderer software;
    std::string error;
    REQUIRE(GameSceneManager.LoadGame(path, error));
    TestEnvironment::RunFrames(2, 16.0f);
    // The pulse: the colour changes over time, and stays red
    float lowest = 10.0f, highest = 0.0f;
    for (int frame = 0; frame < 12; ++frame)
    {
        TestEnvironment::RunFrames(3, 16.0f);
        AppStub::Pixel p = PixelOf({0, 0.25f, 0});
        CHECK(p.R > p.B);
        lowest = std::min(lowest, p.R);
        highest = std::max(highest, p.R);
    }
    CHECK(highest > lowest + 0.1f);
    std::filesystem::remove(path);
}

TEST_CASE("Render: the hardware triangles are coloured by each object's fragment shader")
{
    Entity square = LitSquare();
    TestEnvironment::RunFrame(16.0f);
    auto colors = [] {
        std::vector<Vec3> all;
        for (const auto& t : AppStub::Get().Triangles)
            for (const Vec3& c : t.Color)
                all.push_back(c);
        return all;
    };
    // Shape shader: shades of the white square
    for (const Vec3& c : colors())
        CHECK(Near(c.X, c.Z, 0.01f));
    // The Red shader turns every corner red
    SceneObjects::SetFragmentShader(square, RedShaderID);
    TestEnvironment::RunFrames(2, 16.0f);
    REQUIRE(!colors().empty());
    for (const Vec3& c : colors())
        CHECK(Near(c, Vec3(1, 0, 0)));
    // The Pulse shader animates the corner colours
    SceneObjects::SetFragmentShader(square, PulseShaderID);
    TestEnvironment::RunFrames(2, 16.0f);
    float first = colors().front().X;
    bool changed = false;
    for (int i = 0; i < 20 && !changed; ++i)
    {
        TestEnvironment::RunFrames(2, 16.0f);
        changed = std::fabs(colors().front().X - first) > 0.05f;
    }
    CHECK(changed);
}
