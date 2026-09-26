//---------------------------------------------------------------------------------
// MIPrefabs.h
//---------------------------------------------------------------------------------
//
// Everything Metal Invasion spawns at runtime, the counterpart of the
// original Prefabs.cpp. Each prefab is an ordinary scene object built with
// SceneObjects (Transform, SceneObject, Mesh or Shape2D, RigidBody) plus a
// ScriptComponent naming its behaviour: the ScriptSystem starts the script
// on the next frame, exactly as for an object placed in the editor.
//
#pragma once

#include "Entity.h"
#include "Vec3.h"

#include <string>

namespace MI
{
    // Side an object fights for, derived from its tag (see MINames.h)
    enum class Side
    {
        Player,
        Enemy,
        Neutral
    };

    Side SideOfTag(const std::string& tag);
    Side SideOf(Entity entity);

    // Player units (spawned in front of the base when purchased)
    Entity SpawnSoldier(const Vec3& position, int battalion);
    Entity SpawnSupport(const Vec3& position, int battalion);
    Entity SpawnTank(const Vec3& position, int battalion);
    // A square formation of `count` units centred on `position`
    void SpawnBattalion(const Vec3& position, int count, int battalion, bool support);

    // Enemies
    Entity SpawnEnemySoldier(const Vec3& position, float speed, float health);
    Entity SpawnEnemyTank(const Vec3& position, float health);
    void SpawnEnemyBattalion(const Vec3& position, int count, float speed, float health);

    // World
    Entity SpawnCrystal(const Vec3& position, int amount);
    // Wall as placed by the player: a static 2D rectangle blocking paths.
    // `ghost` = the preview following the mouse (no body, no script yet)
    Entity SpawnWall(const Vec3& position, bool rotated, bool ghost);
    // Turn a ghost wall into a real one (body, obstacle, script)
    void BuildWall(Entity ghost, bool rotated);

    // Effects
    Entity SpawnLaser(const Vec3& from, const Vec3& to, const Vec3& color);
    Entity SpawnBullet(const Vec3& position, float yawDegrees, Side side);
    // Damages the other side of `side`; Side::Neutral = only visual
    Entity SpawnExplosion(const Vec3& position, Side side);

    // Sizes (models are normalised to a 1 unit footprint, see AssetServer)
    constexpr float BASE_SCALE = 3.0f;
    constexpr float SOLDIER_SCALE = 0.45f;
    constexpr float SUPPORT_SCALE = 0.5f;
    constexpr float TANK_SCALE = 1.1f;
    constexpr float CRYSTAL_SCALE = 1.3f;
    constexpr float WALL_LENGTH = 5.0f;
    constexpr float WALL_WIDTH = 0.6f;
} // namespace MI
