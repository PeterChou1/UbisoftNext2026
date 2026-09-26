//---------------------------------------------------------------------------------
// GameComponents.h
//---------------------------------------------------------------------------------
//
// Example components of the project, edited in the scene editor
// (inspector -> Components) and saved in scene files without any
// serialization code: each REFLECT block below is all it takes.
//
// A tutorial on writing your own is in docs/ComponentsTutorial.md.
//
#pragma once

#include "Entity.h"
#include "Reflection/Reflection.h"
#include "Vec3.h"

#include <string>

/**
 * \brief Hit points. DamageZone scripts remove them, the object is destroyed
 *        at 0 when DestroyAtZero is set
 */
struct Health
{
    float Current = 100.0f;
    float Max = 100.0f;
    bool Invulnerable = false;
    bool DestroyAtZero = true;
};

REFLECT(Health)
{
    Field("Current", &Health::Current).Range(0, 10000).Step(10);
    Field("Max", &Health::Max).Range(1, 10000).Step(10);
    Field("Invulnerable", &Health::Invulnerable).Label("Invuln.");
    Field("DestroyAtZero", &Health::DestroyAtZero).Label("Die at 0");
}

enum class Team
{
    Neutral,
    Player,
    Enemy
};

SERIALIZATION_ENUM_RANGE(Team, Team::Neutral, Team::Enemy)

/**
 * \brief Which side an object is on, with a display name and colour
 */
struct Faction
{
    Team Side = Team::Neutral;
    std::string Title = "Unnamed";
    Vec3 Banner = {0.85f, 0.85f, 0.85f};
    int Rank = 1;
    // Written by scripts while playing, shown but not editable
    int Kills = 0;
};

REFLECT(Faction)
{
    Field("Side", &Faction::Side).Options({"Neutral", "Player", "Enemy"});
    Field("Title", &Faction::Title);
    Field("Banner", &Faction::Banner).AsColor();
    Field("Rank", &Faction::Rank).Range(1, 10);
    Field("Kills", &Faction::Kills).ReadOnly();
}

/**
 * \brief A point of a path: WaypointFollower objects walk from waypoint to
 *        waypoint, following Next. A follower's own Waypoint says where it
 *        goes first
 */
struct Waypoint
{
    Entity Next = NULL_ENTITY;
    float WaitSeconds = 0.5f;
};

REFLECT(Waypoint)
{
    Field("Next", &Waypoint::Next).AsEntity().Tooltip("Object to go to next");
    Field("WaitSeconds", &Waypoint::WaitSeconds).Label("Wait s").Range(0, 60).Step(0.5);
}

namespace ComponentNames
{
    // Stored in scene files: never rename
    constexpr const char* Health = "Health";
    constexpr const char* Faction = "Faction";
    constexpr const char* Waypoint = "Waypoint";
} // namespace ComponentNames

/**
 * \brief Register the components above (called by RegisterGameScripts)
 */
void RegisterGameComponents();
