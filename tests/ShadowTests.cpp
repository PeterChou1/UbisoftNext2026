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

    // A wide grey ground and a tall pillar at the origin, lit from -Z at 45
    // degrees (the shadow falls on z = 1 .. 9), seen from the +Z side
    void ShadowScene(FragShaderTypeID groundShader, SceneLightType type)
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

        SceneLighting::Settings light;
        light.Light.Type = type;
        light.Light.Pitch = 45.0f;
        // (a spot light far enough back to cover the field)
        light.Position = {0.0f, 40.0f, -40.0f};
        SceneLighting::Create(light);
        // Seen through a camera object, like a played scene
        SceneCamera::View view;
        view.Yaw = 180.0f;
        view.Distance = 30.0f;
        SceneCamera::Create(view);
        TestEnvironment::Player().OnWorldRestored();
    }
} // namespace

TEST_CASE("Shadows: a pillar casts a shadow on the ground behind it (directional and spot lights)")
{
    for (SceneLightType type : {SceneLightType::Directional, SceneLightType::Spot})
    {
        for (FragShaderTypeID shader : {ShapeShaderID, PulseShaderID, RimShaderID, BlinnPhongID})
        {
            ShadowScene(shader, type);
            const Vec3 shaded = {0.0f, 0.0f, 5.0f};
            const Vec3 lit = {6.0f, 0.0f, 5.0f};
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
                float b = Brightness(lit);
                // All along the shadow, from right behind the pillar (the
                // shadow is attached to it) to near its far end
                for (float z : {1.3f, 3.0f, 5.0f, 7.0f, 8.5f})
                {
                    float a = Brightness({0.0f, 0.0f, z});
                    CHECK(a > 0.0f);
                    CHECK(a < 0.8f * b);
                }
                // Past its end (a spot light's rays spread: its shadow is
                // longer), and beside it: lit
                float end = type == SceneLightType::Directional ? 10.5f : 12.5f;
                CHECK(Brightness({0.0f, 0.0f, end}) > 0.8f * b);
                CHECK(Brightness({2.5f, 0.0f, 5.0f}) > 0.8f * b);
                CHECK(Brightness({-6.0f, 0.0f, -6.0f}) > 0.8f * b);
            }
        }
    }
}

TEST_CASE("Shadows: open ground under a directional light has no shadow acne")
{
    // A directional light lights flat ground the same everywhere: any
    // darker pixel away from the pillar's shadow is an artifact
    for (float pitch : {80.0f, 45.0f, 20.0f})
    {
        ShadowScene(ShapeShaderID, SceneLightType::Directional);
        ECS.GetComponent<SceneLight>(SceneLighting::Find()).Pitch = pitch;
        Renderer software(true);
        TestEnvironment::RunFrames(2, 16.0f);
        // Where the shadow falls: x in -1 .. 1, z from 1 to the pillar's
        // top along the light
        float length = 8.0f / std::tan(pitch * 3.14159265f / 180.0f) + 1.5f;
        float reference = Brightness({8.0f, 0.0f, 0.0f});
        REQUIRE(reference > 0.1f);
        int checked = 0;
        for (float x = -12.0f; x <= 12.0f; x += 0.75f)
        {
            for (float z = -12.0f; z <= 12.0f; z += 0.75f)
            {
                // The shadow, and the ground the pillar hides from the camera
                // (it looks from +Z)
                if (std::fabs(x) < 2.5f && z < 1.0f + length)
                    continue;
                float b = Brightness({x, 0.0f, z});
                if (b <= 0.0f)
                    continue; // off the screen
                ++checked;
                CHECK(std::fabs(b - reference) < 0.03f);
            }
        }
        CHECK(checked > 500);
    }
}

TEST_CASE("Shadows: a spot light does not light what its cone (and its shadows) do not cover")
{
    ShadowScene(ShapeShaderID, SceneLightType::Spot);
    Entity light = SceneLighting::Find();
    ECS.GetComponent<SceneLight>(light).Spread = 30.0f;
    SceneObjects::SetPosition(light, {0.0f, 20.0f, -20.0f});
    Renderer software(true);
    TestEnvironment::RunFrames(2, 16.0f);
    // Its axis meets the ground at the origin; 12 units to the side is far
    // outside a 30 degree cone: only the ambient light is left there
    float centre = Brightness({3.0f, 0.0f, 0.0f});
    float outside = Brightness({12.0f, 0.0f, 0.0f});
    CHECK(centre > outside + 0.3f);
    CHECK(std::fabs(outside - 3.0f * 0.8f * 0.45f) < 0.05f);
}
