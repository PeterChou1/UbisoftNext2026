#include "SceneLight.h"

#include "../ECSManager.h"
#include "../GameOptions.h"
#include "../Lighting.h"
#include "../Transform.h"
#include "SceneComponents.h"
#include "SceneObjects.h"

#include <algorithm>
#include <cmath>

extern ECSManager ECS;

namespace SceneLighting
{
    namespace
    {
        constexpr float TO_RADIANS = 3.14159265358979f / 180.0f;
        // How far along the light's direction the "target" is placed when
        // it does not reach the ground
        constexpr float FAR_TARGET = 30.0f;
    } // namespace

    Vec3 Direction(float yawDegrees, float pitchDegrees)
    {
        float yaw = yawDegrees * TO_RADIANS;
        float pitch = std::clamp(pitchDegrees, -89.0f, 89.0f) * TO_RADIANS;
        return {std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    }

    Vec3 GroundTarget(const Settings& settings)
    {
        Vec3 direction = Direction(settings.Yaw, settings.Light.Pitch);
        if (direction.Y < -1e-3f && settings.Position.Y > 0.0f)
            return settings.Position + direction * (settings.Position.Y / -direction.Y);
        return settings.Position + direction * FAR_TARGET;
    }

    Entity Find()
    {
        // Visit returns the entities in order: the first is the lowest id
        for (Entity e : ECS.Visit<SceneLight, Transform>())
            return e;
        return NULL_ENTITY;
    }

    Settings SettingsOf(Entity light)
    {
        Settings settings;
        settings.Position = SceneObjects::GetPosition(light);
        settings.Yaw = SceneObjects::GetYaw(light);
        settings.Light = ECS.GetComponent<SceneLight>(light);
        return settings;
    }

    Settings Current()
    {
        Entity light = Find();
        return light != NULL_ENTITY ? SettingsOf(light) : Settings{};
    }

    void Apply(Lighting& lighting, const Settings& settings)
    {
        const SceneLight& light = settings.Light;
        Vec3 position = settings.Position;
        Vec3 target = position + Direction(settings.Yaw, light.Pitch);
        lighting.SetLightPerspective(std::clamp(light.Spread, 1.0f, 170.0f), 1.0f, 0.1f, 1000.0f);
        lighting.SetPositionAndTarget(position, target);
        DirectionalLight& directional = lighting.GetDirectionalLight();
        directional.Color = Vec3(std::clamp(light.Color.X, 0.0f, 1.0f),
                                 std::clamp(light.Color.Y, 0.0f, 1.0f),
                                 std::clamp(light.Color.Z, 0.0f, 1.0f));
        directional.Intensity = std::max(light.Intensity, 0.0f);
        directional.Ambient = std::clamp(light.Ambient, 0.0f, 1.0f);
    }

    void Update(Lighting& lighting, GameOptions& options)
    {
        Settings settings = Current();
        Apply(lighting, settings);
        options.LightShadows = settings.Light.Shadows;
    }

    Entity Create(const Settings& settings, const std::string& name)
    {
        Entity e = SceneObjects::CreateEmpty(name, settings.Position, settings.Yaw);
        ECS.GetComponent<SceneObject>(e).Tag = TAG;
        ECS.AddComponent<SceneLight>(e, settings.Light);
        return e;
    }
} // namespace SceneLighting
