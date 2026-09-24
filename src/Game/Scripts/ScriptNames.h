//---------------------------------------------------------------------------------
// ScriptNames.h
//---------------------------------------------------------------------------------
//
// Registered names of the project's scripts. These strings are stored in scene
// files (ScriptComponent / SceneSettings): renaming one breaks existing scenes.
//
#pragma once

namespace ScriptNames
{
    // Object scripts
    constexpr const char* Rotator = "Rotator";
    constexpr const char* Patrol = "Patrol";
    constexpr const char* Mover = "Mover";
    constexpr const char* PlayerController = "PlayerController";
    constexpr const char* Follower = "Follower";
    constexpr const char* Collectible = "Collectible";
    constexpr const char* Hazard = "Hazard";
    constexpr const char* MovingHazard = "MovingHazard";
    constexpr const char* Projectile = "Projectile";
    constexpr const char* Spawner = "Spawner";
    // Scene scripts
    constexpr const char* CollectGame = "CollectGame";
} // namespace ScriptNames
