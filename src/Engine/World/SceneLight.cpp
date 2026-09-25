#include "SceneLight.h"

#include "../DepthBuffer.h"
#include "../ECSManager.h"
#include "../GameOptions.h"
#include "../Lighting.h"
#include "../Transform.h"
#include "../VertexBuffer.h"
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

    bool SceneBounds(Vec3& min, Vec3& max)
    {
        if (!ECS.HasResource<VertexBuffer>())
            return false;
        const std::vector<Vertex>& vertices = ECS.GetResource<VertexBuffer>()->Buffer;
        if (vertices.empty())
            return false;
        min = max = vertices.front().Position;
        for (const Vertex& v : vertices)
        {
            min = Vec3(std::min(min.X, v.Position.X), std::min(min.Y, v.Position.Y), std::min(min.Z, v.Position.Z));
            max = Vec3(std::max(max.X, v.Position.X), std::max(max.Y, v.Position.Y), std::max(max.Z, v.Position.Z));
        }
        return true;
    }

    void Apply(Lighting& lighting, const Settings& settings)
    {
        Vec3 min, max;
        if (!SceneBounds(min, max))
        {
            min = Vec3(-20.0f, -1.0f, -20.0f);
            max = Vec3(20.0f, 5.0f, 20.0f);
        }
        Apply(lighting, settings, min, max);
    }

    void Apply(Lighting& lighting, const Settings& settings, const Vec3& sceneMin, const Vec3& sceneMax)
    {
        const SceneLight& light = settings.Light;
        DirectionalLight& directional = lighting.GetDirectionalLight();
        Vec3 direction = Direction(settings.Yaw, light.Pitch);
        // Shadow map size, for the texel sizes of the lookups
        float texels = static_cast<float>(APP_VIRTUAL_HEIGHT);
        if (ECS.HasResource<DepthBuffer>())
            texels = static_cast<float>(std::min(ECS.GetResource<DepthBuffer>()->ShadowTexelsX(),
                                                 ECS.GetResource<DepthBuffer>()->ShadowTexelsY()));

        if (light.Type == SceneLightType::Directional)
        {
            // A sun: look at the scene's centre from far enough along the
            // direction, and fit the orthographic box around every vertex
            Vec3 center = (sceneMin + sceneMax) * 0.5f;
            Vec3 half = (sceneMax - sceneMin) * 0.5f;
            float radius = std::sqrt(half.Dot(half)) + 1.0f;
            Vec3 eye = center - direction * (radius + 10.0f);
            lighting.SetPositionAndTarget(eye, center);
            float left = 1e30f, right = -1e30f, bottom = 1e30f, top = -1e30f, nearest = 1e30f, farthest = -1e30f;
            for (int corner = 0; corner < 8; ++corner)
            {
                Vec3 p((corner & 1) ? sceneMax.X : sceneMin.X,
                       (corner & 2) ? sceneMax.Y : sceneMin.Y,
                       (corner & 4) ? sceneMax.Z : sceneMin.Z);
                Vec3 local = directional.WorldToLightSpace(p);
                left = std::min(left, local.X);
                right = std::max(right, local.X);
                bottom = std::min(bottom, local.Y);
                top = std::max(top, local.Y);
                // The light looks down its -Z axis
                nearest = std::min(nearest, -local.Z);
                farthest = std::max(farthest, -local.Z);
            }
            // A margin, and edges on a whole unit grid so the box (and the
            // shadows' edges) do not shimmer while objects move a little
            left = std::floor(left - 1.0f);
            bottom = std::floor(bottom - 1.0f);
            right = std::ceil(right + 1.0f);
            top = std::ceil(top + 1.0f);
            nearest = std::max(0.1f, nearest - 2.0f);
            farthest += 2.0f;
            directional.SetOrthographic(left, right, bottom, top, nearest, farthest);
            directional.TexelSize = std::max(right - left, top - bottom) / texels;
            directional.DepthPerUnit = 1.0f / (farthest - nearest);
            directional.SpotCosInner = directional.SpotCosOuter = -1.0f;
        }
        else
        {
            float spread = std::clamp(light.Spread, 1.0f, 170.0f);
            lighting.SetLightPerspective(spread, 1.0f, 0.1f, 1000.0f);
            lighting.SetPositionAndTarget(settings.Position, settings.Position + direction);
            constexpr float HALF_TO_RADIANS = 0.5f * TO_RADIANS;
            // Fades over the outer fifth of the cone, gone at its edge
            directional.SpotCosOuter = std::cos(spread * HALF_TO_RADIANS);
            directional.SpotCosInner = std::cos(spread * 0.8f * HALF_TO_RADIANS);
            directional.TexelSize = 2.0f * std::tan(spread * HALF_TO_RADIANS) / texels;
        }
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
