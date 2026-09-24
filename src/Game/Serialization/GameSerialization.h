//---------------------------------------------------------------------------------
// GameSerialization.h
//---------------------------------------------------------------------------------
//
// Serialize functions for the gameplay components/resources of Metal Invasion
// and the registry describing everything that goes into a save file.
//
// What is saved
//   Components : Transform, RigidBody, Mesh, FragShaderTag, VertShaderTag,
//                Particle, Emitter, UITarget, AIObstacle, PlayerControlUnit,
//                BasicEnemyUnit, CrystalDeposit, PlayerBase, TankBullet,
//                Explosion, LaserProjectile
//   Resources  : GameState (round, timers, crystals, difficulty, camera state)
//                BlackBoard (AI targets / memory + vector field configuration)
//                UIState (interaction mode only, e.g. placing a wall)
//
// What is NOT saved (rebuilt after loading, see MainLevel::OnWorldRestored)
//   - BehaviorTree components and the behaviour tree node graphs
//   - The vector field grids (recomputed from AIObstacle entities)
//   - Render caches (vertex/index buffers, shader instances)
//   - Engine resources which are configuration or per frame scratch data
//     (Camera, Lighting, buffers, GameOptions, UIState mouse / widget state)
//
#pragma once

#include "../BasicEnemyUnit.h"
#include "../BlackBoard.h"
#include "../Bullet.h"
#include "../Crystal.h"
#include "../GameState.h"
#include "../Laser.h"
#include "../PlayerBase.h"
#include "../PlayerUnits.h"
#include "../UIState.h"
#include "EngineSerialization.h"
#include "SerializationRegistry.h"

template <typename Archive>
void Serialize(Archive& ar, PlayerControlUnit& unit)
{
    ar(unit.battalionId, unit.health, unit.selected, unit.isTank, unit.isWall);
}

template <typename Archive>
void Serialize(Archive& ar, BasicEnemyUnit& unit)
{
    ar(unit.health, unit.Speed);
}

template <typename Archive>
void Serialize(Archive& ar, CrystalDeposit& deposit)
{
    ar(deposit.AmountOfCrystal);
}

template <typename Archive>
void Serialize(Archive& ar, PlayerBaseComponent& base)
{
    ar(base.PlayerBaseHealth);
}

template <typename Archive>
void Serialize(Archive& ar, TankBullet& bullet)
{
    ar(bullet.fuseTime);
}

template <typename Archive>
void Serialize(Archive& ar, Explosion& explosion)
{
    ar(explosion.Duration, explosion.GrowthFactor);
}

template <typename Archive>
void Serialize(Archive& ar, LaserProjectile& laser)
{
    ar(laser.LaserTime);
}

template <typename Archive>
void Serialize(Archive& ar, GameState& state)
{
    // Cursor / round flow
    ar(state.ObstacleInCursor, state.OffscreenPosition);
    ar(state.PrepPhaseTime, state.InvasionPhaseTime, state.CurrentTimeInvasion,
       state.CurrentTimePrep);
    ar(state.battalionCount, state.PlayerCrystalInventory, state.RoundNumber, state.currentState);
    // Difficulty
    ar(state.enemySpeedUpper, state.enemySpeedLower, state.enemyHealth, state.spawnDistance);
    ar(state.currentInterval, state.SpawnVolume, state.SpawnInterval, state.EnemyToTankRatio);
    // Camera
    ar(state.StartPosition, state.StartCamera, state.StartCameraTarget);
    ar(state.CurCameraState, state.CameraEndState, state.TransformStart, state.TransformEnd);
    ar(state.CameraFollow, state.LerpProgress, state.LerpTime);
}

/**
 * \brief Only the configuration of a vector field is saved, the grid itself is
 *        derived data rebuilt from the obstacles after loading
 */
template <typename Archive>
void SerializeVectorFieldConfig(Archive& ar, VectorField& field)
{
    ar(field.LocationVectorField, field.GridCountHeight, field.GridCountWidth);
    ar(field.HalfHeight, field.HalfWidth);
}

template <typename Archive>
void Serialize(Archive& ar, BlackBoard& board)
{
    ar(board.UnitTarget, board.EnemyTarget);
    SerializeVectorFieldConfig(ar, board.UnitVectorField);
    SerializeVectorFieldConfig(ar, board.EnemyVectorField);
    ar(board.PlayerTankTargets, board.EnemyTankTargets);
    ar(board.LastKnownLocation, board.InLineOfSight, board.PatrolTargets);
    ar(board.DeltaTime);
}

/**
 * \brief Only the interaction mode is gameplay state: a wall being placed has
 *        already been paid for, the save must keep the game in build mode with
 *        the same orientation. Mouse / widget state is per frame input data
 */
template <typename Archive>
void Serialize(Archive& ar, UIState& ui)
{
    ar(ui.state, ui.flipped);
}

namespace Serialization
{
    /**
     * \brief Register engine level components (transform, physics, rendering ...)
     */
    void RegisterEngineSerializers(SerializationRegistry& registry);

    /**
     * \brief Register gameplay components and resources
     */
    void RegisterGameSerializers(SerializationRegistry& registry);

    /**
     * \brief Registry containing every engine + game serializer, built once
     */
    const SerializationRegistry& GetGameSerializationRegistry();
} // namespace Serialization
