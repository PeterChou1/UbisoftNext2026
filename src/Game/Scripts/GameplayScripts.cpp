#include "GameplayScripts.h"

#include "CollectGame.h"
#include "ScriptNames.h"
#include "World/SceneComponents.h"

#include <algorithm>

void Collectible::OnUpdate(float deltaSeconds)
{
    SetYaw(Yaw() + Param("SpinSpeed") * deltaSeconds);
}

void Collectible::OnCollisionEnter(Entity other)
{
    if (TagOf(other) != "Player")
        return;
    if (auto* game = SceneScriptAs<CollectGame>())
        game->AddScore(static_cast<int>(Param("Points")));
    DestroySelf();
}

void Hazard::OnCollisionEnter(Entity other)
{
    if (TagOf(other) != "Player")
        return;
    // Without the CollectGame rules, touching a hazard restarts the scene
    if (auto* game = SceneScriptAs<CollectGame>())
        game->PlayerHit();
    else
        RestartScene();
}

void MovingHazard::OnCollisionEnter(Entity other)
{
    if (TagOf(other) != "Player")
        return;
    if (auto* game = SceneScriptAs<CollectGame>())
        game->PlayerHit();
    else
        RestartScene();
}

void Projectile::OnCollisionEnter(Entity other)
{
    if (TagOf(other) != "Player")
        return;
    if (auto* game = SceneScriptAs<CollectGame>())
        game->PlayerHit();
    DestroySelf();
}

void Spawner::OnUpdate(float deltaSeconds)
{
    m_Timer += deltaSeconds;
    float interval = std::max(Param("Interval"), 0.1f);
    if (m_Timer < interval)
        return;
    m_Timer -= interval;

    // Built exactly like the editor builds objects, with its own script
    SceneObjects::ShapeDesc shot;
    shot.Name = NameOf(Self()) + " shot " + std::to_string(++m_Spawned);
    shot.Tag = "Hazard";
    shot.Shape.Type = Shape2DType::Circle;
    shot.Shape.Width = 0.5f;
    shot.Shape.Thickness = 0.3f;
    shot.Shape.Color = Vec3(0.9f, 0.25f, 0.2f);
    shot.Position = Position();
    shot.YawDegrees = Yaw();
    shot.Body = SceneObjects::BodyType::Trigger;
    shot.Script = ScriptNames::Projectile;
    shot.ScriptParams = {{"Speed", Param("Speed")}, {"Lifetime", Param("Lifetime")}};
    Spawn(shot);
}
