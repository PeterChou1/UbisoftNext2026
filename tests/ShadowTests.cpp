//---------------------------------------------------------------------------------
// ShadowTests.cpp
//---------------------------------------------------------------------------------
//
// Shadow maps: with GameOptions::ShadowMapping, the software renderer draws the
// scene from the light, and the lit shaders darken what something closer to
// the light hides
//
#include "AppStub.h"
#include "Camera.h"
#include "GameOptions.h"
#include "Lighting.h"
#include "WorldFixture.h"
#include "World/SceneCamera.h"
#include "World/SceneLight.h"

#include <cmath>

namespace
{
    struct Renderer
    {
        Renderer(bool shadows)
        {
            auto options = ECS.GetResource<GameOptions>();
            options->LineRendering = false;
            options->ShadowMapping = shadows;
        }
        ~Renderer()
        {
            auto options = ECS.GetResource<GameOptions>();
            options->LineRendering = true;
            options->ShadowMapping = false;
        }
    };

    float Brightness(const Vec3& world)
    {
        Vec2 s = ECS.GetResource<Camera>()->WorldPointToScreenSpace(world);
        AppStub::Pixel p = AppStub::PixelAt(static_cast<int>(s.X), static_cast<int>(s.Y));
        return p.R + p.G + p.B;
    }

    // A wide grey ground and a tall pillar at the origin; the light is above
    // and behind (-Z), the camera looks from the other side (+Z)
    void ShadowScene(FragShaderTypeID groundShader)
    {
        Fixture::FreshWorld();
        AppStub::Reset();
        SceneObjects::ShapeDesc ground = Fixture::ShapeOf("Ground", Shape2DType::Rectangle, {0, -0.1f, 0});
        ground.Shape.Width = 30.0f;
        ground.Shape.Height = 30.0f;
        ground.Shape.Thickness = 0.1f;
        ground.Shape.Color = {0.8f, 0.8f, 0.8f};
        Entity g = SceneObjects::CreateShape(ground);
        SceneObjects::SetFragmentShader(g, groundShader);
        SceneObjects::ShapeDesc pillar = Fixture::ShapeOf("Pillar", Shape2DType::Rectangle, {0, 0, 0});
        pillar.Shape.Width = 2.0f;
        pillar.Shape.Height = 2.0f;
        pillar.Shape.Thickness = 8.0f;
        SceneObjects::CreateShape(pillar);

        // The light: a light object where the old fixed light was
        SceneLighting::Create();
        // Seen through a camera object, like a played scene
        SceneCamera::View view;
        view.Yaw = 180.0f;
        view.Distance = 30.0f;
        SceneCamera::Create(view);
        TestEnvironment::Player().OnWorldRestored();
    }
} // namespace

TEST_CASE("Shadows: a pillar casts a shadow on the ground behind it")
{
    for (FragShaderTypeID shader : {ShapeShaderID, PulseShaderID, RimShaderID, BlinnPhongID})
    {
        ShadowScene(shader);
        // Behind the pillar (seen from the light) and to the side of it
        const Vec3 shaded = {0.0f, 0.0f, 2.8f};
        const Vec3 lit = {6.0f, 0.0f, 2.8f};
        {
            Renderer software(false);
            TestEnvironment::RunFrames(2, 16.0f);
            float a = Brightness(shaded);
            float b = Brightness(lit);
            CHECK(a > 0.05f);
            // Without shadows the two spots are lit about the same
            CHECK(std::fabs(a - b) < 0.25f * b);
        }
        {
            Renderer software(true);
            TestEnvironment::RunFrames(2, 16.0f);
            float a = Brightness(shaded);
            float b = Brightness(lit);
            CHECK(a > 0.0f);
            CHECK(a < 0.8f * b);
            // Open ground far from the pillar has no shadow acne
            CHECK(Brightness({-6.0f, 0.0f, -6.0f}) > 0.8f * b);
            CHECK(Brightness({6.0f, 0.0f, 8.0f}) > 0.8f * b);
        }
    }
}
