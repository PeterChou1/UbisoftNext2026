#pragma once
#include "GameOptions.h"
#include "Resource.h"

enum GameCameraState
{
    StartMenu,
    OptionsMenu,
    LerpToPosition
};

enum RoundState
{
    // Spawn Random Terrain Object
    Spawn,
    // Preperation (no enemy spawn)
    Prepartion,
    // Invasion
    Invasion
};

class GameState : public Resource
{
  public:
    void ResetResource() override
    {
        RoundNumber = 1;
        SpawnVolume = 1;
        PlayerCrystalInventory = 25;
        currentState = Spawn;
        currentInterval = 0.0f;
        CurrentTimePrep = 0.0f;
        CurrentTimeInvasion = 0.0f;
        InvasionPhaseTime = 20.0f;
    }

    // Cursor
    Entity ObstacleInCursor = NULL_ENTITY;
    // Offscreen point to store unused components
    Vec3 OffscreenPosition = {0, -2, 0};
    float PrepPhaseTime = 60.0f;
    float InvasionPhaseTime = 20.0f;
    float CurrentTimeInvasion = 0.0f;
    float CurrentTimePrep = 0.0f;
    int battalionCount = 0;
    int PlayerCrystalInventory = 25;
    int RoundNumber = 1;
    RoundState currentState = Spawn;
    // enemy stats increased every round
    float enemySpeedUpper = 0.0015f;
    float enemySpeedLower = 0.001f;
    int enemyHealth = 100;
    float spawnDistance = 25.0f;
    float currentInterval = 0.0f;
    // Controls how many enemys spawn per interval
    int SpawnVolume = 3;
    // Controls time per interval (Default every 20 sec)
    float SpawnInterval = 15.0f;
    // how likely you are to spawn a soldier vs a tank
    float EnemyToTankRatio = 0.7f;

    Vec3 StartPosition{};
    Vec3 StartCamera{};
    Vec3 StartCameraTarget{};
    GameCameraState CurCameraState;
    // --- used for lerping --
    GameCameraState CameraEndState;
    Transform TransformStart;
    Transform TransformEnd;
    Vec3 CameraFollow;
    float LerpProgress;
    float LerpTime;

    void LerpToTargetCamera(float time, Transform& start, Transform& target, GameCameraState state)
    {
        CurCameraState = LerpToPosition;
        CameraEndState = state;
        TransformStart = start;
        TransformEnd = target;
        LerpTime = time;
        LerpProgress = 0;
    }

    void LerpToTarget(float time, Transform& start, Transform& target)
    {
        TransformStart = start;
        TransformEnd = target;
        LerpTime = time;
        LerpProgress = 0;
    }
};
