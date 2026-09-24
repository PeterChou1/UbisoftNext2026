//---------------------------------------------------------------------------------
// Prefabs.h
//---------------------------------------------------------------------------------
//
// Builders for every placeable Metal Invasion object. They create the entity
// and its data components only (no behaviour trees, no asset loading) so they
// can be used by the game, by the scene editor and by headless unit tests.
//
// The game's Create* functions call these and then attach AI, which
// guarantees that scenes authored in the editor contain exactly the same
// entities the game spawns itself.
//
#pragma once

#include "Assets.h"
#include "Entity.h"
#include "Vec3.h"

namespace Prefabs
{
    // Half size of the playable area (matches the AI vector fields)
    constexpr float PLAY_AREA_HALF_SIZE = 25.0f;

    Entity SpawnGround();

    Entity SpawnPlayerBase(const Vec3& position, int health = 1000);

    Entity SpawnUnitSelector(const Vec3& position);

    Entity SpawnSoldier(const Vec3& position, int health, int battalionId);

    Entity SpawnSupport(const Vec3& position, int health, int battalionId);

    /**
     * \brief Player tank: a root entity (physics + unit data) with a base and a
     *        cannon mesh as children. Returns the root
     */
    Entity SpawnPlayerTank(const Vec3& position, int battalionId, int health = 200);

    Entity SpawnEnemySoldier(const Vec3& position, int health, float speed);

    /**
     * \brief Enemy tank, same layout as the player tank. Returns the root
     */
    Entity SpawnEnemyTank(const Vec3& position, int health = 200);

    Entity SpawnCrystal(const Vec3& position, int amount = 30);

    /**
     * \brief A wall that has been placed by the player (static, AI obstacle)
     * \param flipped wall rotated by 90 degrees
     */
    Entity SpawnWall(const Vec3& position, bool flipped);

    /**
     * \brief First child of parent whose mesh is asset (e.g. a tank's cannon)
     */
    Entity FindChildWithMesh(Entity parent, ObjAsset asset);

    /**
     * \brief Update a wall's orientation and matching AI obstacle footprint
     */
    void SetWallFlipped(Entity wall, bool flipped);
} // namespace Prefabs
