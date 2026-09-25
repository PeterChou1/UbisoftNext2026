//---------------------------------------------------------------------------------
// MINames.h
//---------------------------------------------------------------------------------
//
// Metal Invasion rebuilt on the scene / script engine: names of its scripts
// and of the tags that identify the two sides.
//
//   Player side  "Unit" (soldiers, support, tanks), "Wall", "Base"
//   Enemy side   "Enemy" (enemy soldiers, enemy tanks)
//   Neutral      "Crystal" (mined by support units), "Turret" (tank cannons)
//
#pragma once

namespace MI
{
    namespace Tags
    {
        constexpr const char* Unit = "Unit";
        constexpr const char* Wall = "Wall";
        constexpr const char* Base = "Base";
        constexpr const char* Enemy = "Enemy";
        constexpr const char* Crystal = "Crystal";
        constexpr const char* Turret = "Turret";
    } // namespace Tags

    namespace Scripts
    {
        // Scene script: rounds, economy, selection / orders, base menu, HUD
        constexpr const char* Game = "MetalInvasion";
        // Object scripts
        constexpr const char* Base = "MIBase";
        constexpr const char* Crystal = "MICrystal";
        constexpr const char* Wall = "MIWall";
        constexpr const char* Soldier = "MISoldier";
        constexpr const char* Support = "MISupport";
        constexpr const char* Tank = "MITank";
        constexpr const char* EnemySoldier = "MIEnemySoldier";
        constexpr const char* EnemyTank = "MIEnemyTank";
        constexpr const char* Bullet = "MIBullet";
        constexpr const char* Explosion = "MIExplosion";
    } // namespace Scripts

    // Models (data/models) used by the game
    namespace Models
    {
        constexpr const char* Base = "PlayerBase";
        constexpr const char* Soldier = "Soldier";
        constexpr const char* Support = "SupportUnit";
        constexpr const char* TankHull = "Base";
        constexpr const char* TankCannon = "Cannon";
        constexpr const char* Enemy = "Enemy";
        constexpr const char* Crystal = "Crystal";
        constexpr const char* Bullet = "Bullet";
        constexpr const char* Explosion = "Explosion";
    } // namespace Models
} // namespace MI
