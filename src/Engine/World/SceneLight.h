//---------------------------------------------------------------------------------
// SceneLight.h
//---------------------------------------------------------------------------------
//
// The scene's light is an object like any other: an entity with a Transform
// and a SceneLight component ("Directional Light", tag "Light"). It is listed
// in the editor's hierarchy, moved and turned like the other objects, and
// its SceneLight fields are edited in the inspector.
//
//   SceneLight.Type      Directional: a sun, parallel rays lighting the whole
//                        scene the same way; only its direction matters
//                        (the object's position is only where the editor
//                        shows it). Spot: a cone from where it is
//   Transform yaw        the direction it shines along the ground (0 = +Z)
//   SceneLight.Pitch     how steeply it shines down (90 = straight down)
//   Transform position   Spot: where the light is (its height matters)
//   SceneLight.Spread    Spot: the angle of its cone; the light fades out at
//                        its edge
//   Color / Intensity / Ambient / Shadows
//
// Shadows: a directional light's shadow map is an orthographic box fitted
// around the whole scene every frame (every object casts and receives
// shadows at the same resolution); a spot light's is its cone.
//
// Every frame GameManager applies the first light object (or the default
// light, for scenes without one) to the renderer's Lighting resource, and
// turns the shadow map on or off with its Shadows field.
//
#pragma once

#include "../Entity.h"
#include "../Reflection/Reflection.h"
#include "../Vec3.h"

#include <string>

class GameOptions;
class Lighting;

enum class SceneLightType
{
    Directional,
    Spot
};

/**
 * \brief Makes an object the scene's light (see above)
 */
struct SceneLight
{
    SceneLightType Type = SceneLightType::Directional;
    Vec3 Color = {1.0f, 1.0f, 1.0f};
    // Direct light strength, and the light every surface gets (also in shadow)
    float Intensity = 1.0f;
    float Ambient = 0.45f;
    // Degrees below the horizon; the old fixed light looked down at ~78.7
    float Pitch = 78.69f;
    // Cone angle (degrees): only what it covers is lit by the shadow map
    float Spread = 120.0f;
    bool Shadows = true;
};

REFLECT(SceneLight)
{
    Field("Type", &SceneLight::Type).Options({"Directional", "Spot"}).Tooltip("Directional: a sun. Spot: a cone");
    Field("Color", &SceneLight::Color).AsColor();
    Field("Intensity", &SceneLight::Intensity)
            .Range(0.0f, 3.0f)
            .Step(0.1f)
            .Label("Power")
            .Tooltip("Intensity: strength of the direct light");
    Field("Ambient", &SceneLight::Ambient)
            .Range(0.0f, 1.0f)
            .Step(0.05f)
            .Tooltip("Light every surface gets, also in shadow");
    Field("Pitch", &SceneLight::Pitch).Range(5.0f, 89.0f).Step(5.0f).Tooltip("Degrees shining down");
    Field("Spread", &SceneLight::Spread).Range(30.0f, 150.0f).Step(10.0f).Tooltip("Spot: cone angle (degrees)");
    Field("Shadows", &SceneLight::Shadows).Label("Shadow").Tooltip("Cast shadows (software renderer, Tab)");
}

namespace SceneLighting
{
    constexpr const char* DEFAULT_NAME = "Directional Light";
    constexpr const char* TAG = "Light";

    // Where scenes without a light object are lit from (the old fixed light)
    const Vec3 DEFAULT_POSITION = {0.0f, 25.0f, -5.0f};

    /**
     * \brief Everything that places and colours the light
     */
    struct Settings
    {
        Vec3 Position = DEFAULT_POSITION;
        float Yaw = 0.0f;
        SceneLight Light;
    };

    /**
     * \brief Unit direction the light shines in
     */
    Vec3 Direction(float yawDegrees, float pitchDegrees);

    /**
     * \brief Where the light's centre line meets the ground (y = 0), or a
     *        point along it when it never does
     */
    Vec3 GroundTarget(const Settings& settings);

    /**
     * \brief The scene's light: the first object with a SceneLight and a
     *        Transform, NULL_ENTITY when there is none
     */
    Entity Find();

    /**
     * \brief Settings of a light object (world position / yaw + its SceneLight)
     */
    Settings SettingsOf(Entity light);

    /**
     * \brief The light object's settings, or the defaults
     */
    Settings Current();

    /**
     * \brief Axis aligned box around the scene's geometry (the renderer's
     *        vertex buffer), false when there is none
     */
    bool SceneBounds(Vec3& min, Vec3& max);

    /**
     * \brief Place and colour the renderer's light. A directional light's
     *        shadow box is fitted around sceneMin .. sceneMax
     */
    void Apply(Lighting& lighting, const Settings& settings, const Vec3& sceneMin, const Vec3& sceneMax);
    void Apply(Lighting& lighting, const Settings& settings);

    /**
     * \brief Apply the current light, and tell the renderer whether it casts
     *        shadows (GameOptions::LightShadows). Called every frame
     */
    void Update(Lighting& lighting, GameOptions& options);

    /**
     * \brief Create a light object
     */
    Entity Create(const Settings& settings = {}, const std::string& name = DEFAULT_NAME);
} // namespace SceneLighting
