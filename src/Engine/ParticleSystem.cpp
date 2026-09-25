#include "ParticleSystem.h"

#include "ECSManager.h"
#include "Emitter.h"
#include "FragShaderTag.h"
#include "Transform.h"
#include "stdafx.h"

#include <cmath>
#include <cstdlib>
#include <random>

extern ECSManager ECS;

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;

    float RandomRange(float minVal, float maxVal)
    {
        float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        return minVal + t * (maxVal - minVal);
    }

    Vec3 RandomDirection()
    {
        float rx = RandomRange(-1.0f, 1.0f);
        float ry = RandomRange(-1.0f, 1.0f);
        float rz = RandomRange(-1.0f, 1.0f);
        Vec3 direction(rx, ry, rz);
        direction.Normalize();
        return direction;
    }

    /**
     * \brief A random direction in a cone around direction (half angle in degrees)
     */
    Vec3 RandomDirectionInCone(const Vec3& direction, float coneAngleDegrees)
    {
        Vec3 axis = direction;
        axis.Normalize();

        std::random_device device;
        std::mt19937 generator(device());
        std::uniform_real_distribution<float> azimuthDistribution(0.0f, 2.0f * PI);
        float azimuth = azimuthDistribution(generator);
        std::uniform_real_distribution<float> polarDistribution(0.0f,
                                                                coneAngleDegrees * DEG_TO_RAD);
        float polar = polarDistribution(generator);

        // The random unit vector around the Z axis, then turned onto the axis
        float x = std::sin(polar) * std::cos(azimuth);
        float y = std::sin(polar) * std::sin(azimuth);
        float z = std::cos(polar);

        Vec3 up(0.0f, 0.0f, 1.0f);
        if (std::abs(axis.Dot(up)) > 0.999f)
            up = Vec3(1.0f, 0.0f, 0.0f);
        Vec3 right = up.Cross(axis);
        right.Normalize();
        Vec3 newUp = axis.Cross(right);
        newUp.Normalize();

        return right * x + newUp * y + axis * z;
    }
} // namespace

void ParticleSystem::Update(float deltaTime)
{
    float seconds = deltaTime / 1000.0f;
    for (Entity e : ECS.Visit<Transform, Particle>())
    {
        auto& particle = ECS.GetComponent<Particle>(e);
        particle.duration -= seconds;
        if (particle.duration <= 0.0f)
        {
            ECS.DestroyEntity(e);
            continue;
        }
        ECS.GetComponent<Transform>(e).Update(particle.direction * seconds, Quat());
    }

    for (Entity e : ECS.Visit<Transform, Emitter>())
    {
        auto& emitter = ECS.GetComponent<Emitter>(e);
        emitter.duration -= seconds;
        if (emitter.duration <= 0.0f)
        {
            ECS.DestroyEntity(e);
            continue;
        }

        // density particles per frame, at the emitter
        Vec3 emitterPosition = ECS.GetComponent<Transform>(e).GetWorldPosition();
        for (int i = 0; i < emitter.density; i++)
        {
            Entity particleEntity = ECS.CreateEntity();
            Particle particle;
            particle.direction =
                    emitter.emitterType == Sphere
                            ? RandomDirection()
                            : RandomDirectionInCone(emitter.direction, emitter.coneAngle);
            particle.duration = emitter.particleTime;
            particle.Color = emitter.color;
            // Its quad is added by the MeshHandler
            particle.loaded = false;
            ECS.AddComponent<Particle>(particleEntity, particle);
            Transform transform(
                    emitterPosition, Quat(), Vec3(emitter.size, emitter.size, emitter.size));
            transform.IsDirty = true;
            ECS.AddComponent<Transform>(particleEntity, transform);
            ECS.AddComponent<FragShaderTag>(particleEntity, FragShaderTag(ParticleShaderID));
        }
    }
}
