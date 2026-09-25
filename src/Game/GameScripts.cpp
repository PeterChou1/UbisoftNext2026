#include "GameScripts.h"

#include "Scripting/ScriptRegistry.h"
#include "Scripts/CollectGame.h"
#include "Scripts/Components/ComponentScripts.h"
#include "Scripts/Components/GameComponents.h"
#include "Scripts/GameplayScripts.h"
#include "Scripts/MetalInvasion/MIScripts.h"
#include "Scripts/MovementScripts.h"
#include "Scripts/ScriptNames.h"

void RegisterGameScripts()
{
    ScriptRegistry& r = ScriptRegistry::Get();
    // Object scripts: name, description, {parameter, default, editor step}
    r.Register<Rotator>(ScriptNames::Rotator, "Spins the object", {{"Speed", 90.0f, 15.0f}});
    r.Register<Patrol>(ScriptNames::Patrol,
                       "Moves back and forth along its facing direction",
                       {{"Distance", 3.0f, 0.5f}, {"Speed", 2.0f, 0.5f}});
    r.Register<Mover>(ScriptNames::Mover,
                      "Flies forward and disappears",
                      {{"Speed", 5.0f, 0.5f}, {"Lifetime", 4.0f, 0.5f}});
    r.Register<PlayerController>(
            ScriptNames::PlayerController, "WASD / arrow keys movement", {{"Speed", 6.0f, 0.5f}});
    r.Register<Follower>(ScriptNames::Follower,
                         "Chases the Player when close",
                         {{"Speed", 2.0f, 0.25f}, {"Range", 8.0f, 1.0f}});
    r.Register<Collectible>(ScriptNames::Collectible,
                            "Picked up by the Player for points",
                            {{"Points", 10.0f, 5.0f}, {"SpinSpeed", 90.0f, 15.0f}});
    r.Register<Hazard>(ScriptNames::Hazard, "Hurts the Player");
    r.Register<MovingHazard>(ScriptNames::MovingHazard,
                             "Patrols and hurts the Player",
                             {{"Distance", 3.0f, 0.5f}, {"Speed", 2.0f, 0.5f}});
    r.Register<Projectile>(ScriptNames::Projectile,
                           "Flies forward, hurts the Player",
                           {{"Speed", 6.0f, 0.5f}, {"Lifetime", 3.0f, 0.5f}});
    r.Register<Spawner>(ScriptNames::Spawner,
                        "Fires projectiles in its facing direction",
                        {{"Interval", 2.0f, 0.25f}, {"Speed", 6.0f, 0.5f}, {"Lifetime", 3.0f, 0.5f}});
    // Scripts working with the example components (Scripts/Components)
    r.Register<WaypointFollower>(ScriptNames::WaypointFollower,
                                 "Walks along Waypoint components (Next)",
                                 {{"Speed", 2.0f, 0.5f}});
    r.Register<DamageZone>(ScriptNames::DamageZone,
                           "Removes Health from objects entering it",
                           {{"Damage", 25.0f, 5.0f}});
    // Scene scripts
    r.Register<CollectGame>(ScriptNames::CollectGame,
                            "Collect every Pickup, avoid hazards, next level",
                            {{"Level", 1.0f, 1.0f}, {"Lives", 3.0f, 1.0f}});

    // Metal Invasion, the original game rebuilt on scenes + scripts
    RegisterMetalInvasionScripts(r);

    // Components the editor can add to objects (reflected, saved by name)
    RegisterGameComponents();
}
