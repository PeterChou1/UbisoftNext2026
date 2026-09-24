#include "MovementScripts.h"

#include "World/SceneComponents.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;

    // Direction an object faces on the ground (its +Z rotated by its yaw)
    Vec3 Facing(float yawDegrees)
    {
        float yaw = yawDegrees * DEG_TO_RAD;
        return Vec3(std::sin(yaw), 0.0f, std::cos(yaw));
    }
} // namespace

void Rotator::OnUpdate(float deltaSeconds)
{
    SetYaw(Yaw() + Param("Speed") * deltaSeconds);
}

void Patrol::OnStart()
{
    m_Origin = Position();
    m_Direction = Facing(Yaw());
}

void Patrol::OnUpdate(float deltaSeconds)
{
    float distance = std::max(Param("Distance"), 0.01f);
    m_Travelled += Param("Speed") * deltaSeconds;
    // Triangle wave in [-distance, distance], starting at the authored
    // position (offset 0) and moving forward first
    float period = 4.0f * distance;
    float t = std::fmod(m_Travelled + distance, period);
    float offset = t < 2.0f * distance ? t - distance : 3.0f * distance - t;
    SetPosition(m_Origin + m_Direction * offset);
}

void Mover::OnUpdate(float deltaSeconds)
{
    m_Age += deltaSeconds;
    Vec3 next = Position() + Facing(Yaw()) * (Param("Speed") * deltaSeconds);
    auto settings = Resource<SceneSettings>();
    bool outside = std::fabs(next.X) > settings->FieldWidth * 0.5f ||
                   std::fabs(next.Z) > settings->FieldHeight * 0.5f;
    if (m_Age >= Param("Lifetime") || outside)
    {
        DestroySelf();
        return;
    }
    SetPosition(next);
}

void PlayerController::OnUpdate(float deltaSeconds)
{
    // The camera looks along +Z, so screen right is -X
    Vec3 direction(0, 0, 0);
    if (KeyDown(App::KEY_W) || KeyDown(App::KEY_UP))
        direction.Z += 1.0f;
    if (KeyDown(App::KEY_S) || KeyDown(App::KEY_DOWN))
        direction.Z -= 1.0f;
    if (KeyDown(App::KEY_A) || KeyDown(App::KEY_LEFT))
        direction.X += 1.0f;
    if (KeyDown(App::KEY_D) || KeyDown(App::KEY_RIGHT))
        direction.X -= 1.0f;
    if (direction.GetLengthSqr() > 0.0f)
        direction.Normalize();

    float speed = Param("Speed");
    if (Body() != nullptr && SceneObjects::GetBodyType(Self()) == SceneObjects::BodyType::Dynamic)
        SetVelocity(Vec2(direction.X, direction.Z) * speed);
    else
        SetPosition(Position() + direction * (speed * deltaSeconds));
}

void Follower::OnUpdate(float deltaSeconds)
{
    std::vector<Entity> players = FindByTag("Player");
    if (players.empty())
        return;
    Vec3 toPlayer = PositionOf(players.front()) - Position();
    toPlayer.Y = 0.0f;
    float distance = toPlayer.GetMagnitude();
    if (distance > Param("Range") || distance < 0.01f)
        return;
    Vec3 step = toPlayer * (1.0f / distance) * std::min(Param("Speed") * deltaSeconds, distance);
    SetPosition(Position() + step);
    SetYaw(std::atan2(toPlayer.X, toPlayer.Z) / DEG_TO_RAD);
}
