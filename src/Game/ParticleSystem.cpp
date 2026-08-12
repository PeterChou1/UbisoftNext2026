#include "ParticleSystem.h"

#include "ECSManager.h"
#include "Emitter.h"
#include "FragShaderTag.h"
#include "Transform.h"
#include "stdafx.h"

#include <random>

extern ECSManager ECS;

float RandomRange(float minVal, float maxVal)
{
    float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    return minVal + t * (maxVal - minVal);
}

Vec3 RandomizeDirection()
{
    // e.g. add small random x,y offset
    float spread = 1.0f;
    float rx = RandomRange(-spread, spread);
    float ry = RandomRange(-spread, spread);
    float rz = RandomRange(-spread, spread);
    Vec3 dir(rx, ry, rz);
    dir.Normalize();
    return dir;
}

// Returns a random direction in a cone around 'direction', with cone half-angle in degrees.
Vec3 RandomDirectionInCone(Vec3& direction, float coneAngleDegrees)
{
    Vec3 normalizeDirection = direction;
    normalizeDirection.Normalize();

    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    // Convert cone angle to radians
    float coneAngleRadians = coneAngleDegrees * DEG_TO_RAD;

    // Generate random azimuth angle (0 to 2 * PI)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> azimuthDist(0.0f, 2.0f * PI);

    float azimuth = azimuthDist(gen);

    // Generate random polar angle within the cone
    std::uniform_real_distribution<float> polarDist(0.0f, coneAngleRadians);
    float polar = polarDist(gen);

    // Create a random unit vector in spherical coordinates
    float x = std::sin(polar) * std::cos(azimuth);
    float y = std::sin(polar) * std::sin(azimuth);
    float z = std::cos(polar);

    Vec3 randomVec(x, y, z);

    // Align the random vector with the given direction
    Vec3 up(0.0f, 0.0f, 1.0f);
    if (std::abs(normalizeDirection.Dot(up)) > 0.999f)
        up = Vec3(1.0f, 0.0f, 0.0f);

    Vec3 right = up.Cross(normalizeDirection);
    right.Normalize();
    Vec3 newUp = normalizeDirection.Cross(right);
    newUp.Normalize();

    return right * randomVec.X + newUp * randomVec.Y + normalizeDirection * randomVec.Z;
}

void ParticleSystem::Update(float deltaTime)
{
    float dt = deltaTime / 1000.0f;
    for (auto e : ECS.Visit<Transform, Particle>())
    {
        auto& P = ECS.GetComponent<Particle>(e);
        auto& T = ECS.GetComponent<Transform>(e);
        P.duration -= dt;
        if (P.duration <= 0.0f)
        {
            ECS.DestroyEntity(e);
            continue; // Skip further logic
        }
        T.Update(P.direction * dt, Quat());
    }

    // Visit all entities that have an Emitter component
    for (auto e : ECS.Visit<Transform, Emitter>())
    {
        auto& emitter = ECS.GetComponent<Emitter>(e);

        // Update emitter lifetime
        emitter.duration -= dt;
        if (emitter.duration <= 0.0f)
        {
            ECS.DestroyEntity(e);
            continue;
        }

        // Spawn new Particles based on density
        int toSpawn = emitter.density;

        // If the Emitter has a Transform, we can get that position
        auto& transform = ECS.GetComponent<Transform>(e);
        Vec3 emitterPos = transform.GetWorldPosition();

        for (int i = 0; i < toSpawn; i++)
        {
            // Create a new Particle entity
            Entity particleEntity = ECS.CreateEntity();
            Particle p;
            if (emitter.emitterType == Sphere)
                p.direction = RandomizeDirection();
            else
                p.direction = RandomDirectionInCone(emitter.direction, emitter.coneAngle);

            p.duration = emitter.particleTime;
            p.Color = emitter.color;
            p.loaded = false;
            ECS.AddComponent<Particle>(particleEntity, p);
            Transform particleTransform =
                    Transform(emitterPos, Quat(), Vec3(emitter.size, emitter.size, emitter.size));
            particleTransform.IsDirty = true;
            ECS.AddComponent<Transform>(particleEntity, particleTransform);
            ECS.AddComponent<FragShaderTag>(particleEntity, FragShaderTag(ParticleShaderID));
            // Particle is added in the MeshHandler
        }
    }
}