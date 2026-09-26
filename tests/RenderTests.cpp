//---------------------------------------------------------------------------------
// RenderTests.cpp
//---------------------------------------------------------------------------------
//
// 2D shapes go through the unchanged 3D CPU renderer: a shape's colour must
// show up in the presented frame where the camera projects it
//
#include "AppStub.h"
#include "AssetServer.h"
#include "Camera.h"
#include "GameOptions.h"
#include "WorldFixture.h"

#include <algorithm>
#include <cmath>

namespace
{
    Entity Square(const std::string& name, const Vec3& position, const Vec3& color)
    {
        SceneObjects::ShapeDesc desc = Fixture::ShapeOf(name, Shape2DType::Rectangle, position);
        desc.Shape.Width = 4.0f;
        desc.Shape.Height = 4.0f;
        desc.Shape.Color = color;
        return SceneObjects::CreateShape(desc);
    }

    // Software rasterizer (the frame is presented pixel by pixel) for one test
    struct SoftwareRenderer
    {
        SoftwareRenderer() { ECS.GetResource<GameOptions>()->LineRendering = false; }
        ~SoftwareRenderer() { ECS.GetResource<GameOptions>()->LineRendering = true; }
    };
} // namespace

TEST_CASE("Render: shapes appear in their colour where the camera projects them")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    SoftwareRenderer software;
    auto camera = ECS.GetResource<Camera>();
    SceneObjects::ApplyCamera(*camera, {0, 0, 0}, 20.0f);
    Entity red = Square("Red", {-4, 0, 0}, {0.9f, 0.1f, 0.1f});
    Entity blue = Square("Blue", {4, 0, 0}, {0.1f, 0.1f, 0.9f});
    TestEnvironment::RunFrame(16.0f);
    REQUIRE(ECS.GetComponent<Shape2D>(red).Built);

    Vec2 r = camera->WorldPointToScreenSpace({-4, 0.25f, 0});
    Vec2 b = camera->WorldPointToScreenSpace({4, 0.25f, 0});
    AppStub::Pixel pr = AppStub::PixelAt(static_cast<int>(r.X), static_cast<int>(r.Y));
    AppStub::Pixel pb = AppStub::PixelAt(static_cast<int>(b.X), static_cast<int>(b.Y));
    CHECK(pr.R > 0.3f);
    CHECK(pr.R > 3.0f * pr.G);
    CHECK(pr.R > 3.0f * pr.B);
    CHECK(pb.B > 0.3f);
    CHECK(pb.B > 3.0f * pb.R);

    // Changing the colour rebuilds the mesh
    ECS.GetComponent<Shape2D>(blue).Color = Vec3(0.1f, 0.9f, 0.1f);
    SceneObjects::ShapeChanged(blue);
    TestEnvironment::RunFrame(16.0f);
    pb = AppStub::PixelAt(static_cast<int>(b.X), static_cast<int>(b.Y));
    CHECK(pb.G > 3.0f * pb.B);

    // Removing it clears it from the frame, also when it is removed after
    // the render systems ran (like the editor's Del button during Render)
    GameSceneManager.Update(16.0f);
    SceneObjects::Destroy(blue);
    GameSceneManager.Render();
    ECS.FlushECS();
    TestEnvironment::RunFrame(16.0f);
    pb = AppStub::PixelAt(static_cast<int>(b.X), static_cast<int>(b.Y));
    CHECK(!(pb.G > 3.0f * pb.B && pb.G > 0.3f));
}

TEST_CASE("Render: models and shapes render together")
{
    Fixture::FreshWorld();
    SoftwareRenderer software;
    auto camera = ECS.GetResource<Camera>();
    SceneObjects::ApplyCamera(*camera, {0, 0, 0}, 20.0f);
    Entity crate = SceneObjects::CreateModel("Crate", "Box", {0, 0, 0}, 0.0f, 3.0f);
    Square("Floor", {0, -0.3f, 0}, {0.1f, 0.9f, 0.1f});
    TestEnvironment::RunFrame(16.0f);
    CHECK(ECS.GetComponent<Mesh>(crate).Loaded);
    // Models are normalised to a 1 unit footprint standing on the ground,
    // scale 3 = a 3 x 3 box
    Vec2 c = camera->WorldPointToScreenSpace({0, 1.5f, 0});
    AppStub::Pixel p = AppStub::PixelAt(static_cast<int>(c.X), static_cast<int>(c.Y));
    // The crate hides the green floor behind it
    CHECK(!(p.G > 3.0f * p.R && p.G > 3.0f * p.B));
    CHECK(p.R + p.G + p.B > 0.05f);
}

TEST_CASE("Render: the hardware triangle path uses the shape colours, lit")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    REQUIRE(ECS.GetResource<GameOptions>()->LineRendering);
    auto camera = ECS.GetResource<Camera>();
    SceneObjects::ApplyCamera(*camera, {0, 0, 0}, 20.0f);
    Square("Red", {0, 0, 0}, {0.9f, 0.1f, 0.1f});
    TestEnvironment::RunFrame(16.0f);
    const auto& triangles = AppStub::Get().Triangles;
    REQUIRE(!triangles.empty());
    float brightest = 0.0f, darkest = 1.0f;
    for (const auto& t : triangles)
    {
        for (const Vec3& c : t.Color)
        {
            // Every corner is a shade of the shape colour
            CHECK(c.X > 0.0f);
            CHECK(std::fabs(c.Y / c.X - 0.1f / 0.9f) < 1e-3f);
            brightest = std::max(brightest, c.X);
            darkest = std::min(darkest, c.X);
        }
    }
    // Lit: the top and the side walls do not all get the same shade
    CHECK(brightest > darkest + 0.05f);
    CHECK(brightest <= 0.9f + 1e-4f);
}

TEST_CASE("Render: Tab switches between the hardware and the software renderer")
{
    Fixture::FreshWorld();
    AppStub::Reset();
    auto options = ECS.GetResource<GameOptions>();
    REQUIRE(options->LineRendering);
    AppStub::Get().Keys[App::KEY_TAB] = true;
    TestEnvironment::RunFrame(16.0f);
    AppStub::Get().Keys[App::KEY_TAB] = false;
    CHECK(!options->LineRendering);
    CHECK(AppStub::WasPrinted("software rasterizer"));
    TestEnvironment::RunFrame(16.0f);
    AppStub::Get().Keys[App::KEY_TAB] = true;
    TestEnvironment::RunFrame(16.0f);
    AppStub::Get().Keys[App::KEY_TAB] = false;
    CHECK(options->LineRendering);
    AppStub::Reset();
}

TEST_CASE("Render: models are normalised to a 1 unit footprint on the ground")
{
    for (const std::string& name : AssetServer::AvailableModels())
    {
        const MeshInstance& model = AssetServer::GetInstance().GetModel(name);
        if (model.vertices.empty())
            continue;
        float minX = 1e9f, maxX = -1e9f, minY = 1e9f, minZ = 1e9f, maxZ = -1e9f;
        for (const Vertex& v : model.vertices)
        {
            minX = std::min(minX, v.LocalPosition.X);
            maxX = std::max(maxX, v.LocalPosition.X);
            minY = std::min(minY, v.LocalPosition.Y);
            minZ = std::min(minZ, v.LocalPosition.Z);
            maxZ = std::max(maxZ, v.LocalPosition.Z);
        }
        CHECK(std::fabs(std::max(maxX - minX, maxZ - minZ) - 1.0f) < 1e-3f);
        CHECK(std::fabs(minY) < 1e-4f);
        CHECK(std::fabs(minX + maxX) < 1e-3f);
        CHECK(std::fabs(minZ + maxZ) < 1e-3f);
    }
}
