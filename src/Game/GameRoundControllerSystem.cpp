#include "GameRoundControllerSystem.h"

#include "BasicEnemyUnit.h"
#include "Crystal.h"
#include "ECSManager.h"
#include "GameManager.h"
#include "PlayerBase.h"

extern ECSManager ECS;
extern GameManager GameSceneManager;

static float Rand01()
{
    return float(rand()) / float(RAND_MAX);
}

GameRoundControllerSystem::GameRoundControllerSystem()
{
    m_EnemyController = std::make_shared<EnemyControllerSystem>();
    m_GameState = ECS.GetResource<GameState>();
}

void GameRoundControllerSystem::Update(float deltaTime)
{
    switch (m_GameState->currentState)
    {
    case Spawn: {
        SpawnCrystals();
        break;
    }
    case Prepartion: {
        Preparation(deltaTime);
        break;
    }
    case Invasion: {
        InvadeEnemy(deltaTime);
        break;
    }
    }
    DeleteCrystals();
    CheckGameOver();
}

void GameRoundControllerSystem::CheckGameOver()
{
    Entity E = *ECS.Visit<PlayerBaseComponent>().begin();
    if (ECS.GetComponent<PlayerBaseComponent>(E).PlayerBaseHealth <= 0)
        GameSceneManager.SetActiveScene("WinScreen");
}

void GameRoundControllerSystem::InvadeEnemy(float deltaTime)
{
    m_GameState->CurrentTimeInvasion -= (deltaTime / 1000.0f);
    m_EnemyController->Update(deltaTime);
    if (m_GameState->CurrentTimeInvasion <= 0.0f)
    {
        m_GameState->CurrentTimeInvasion = 0.0f;
        int remainingEnemy = ECS.Visit<BasicEnemyUnit>().size();
        if (remainingEnemy == 0)
        {
            m_GameState->currentState = Spawn;
            m_GameState->currentInterval = 0.0f;
            // Increase difficulty after a round
            m_GameState->RoundNumber += 1;
            m_GameState->SpawnVolume += 2;
        }
    }
}

void GameRoundControllerSystem::Preparation(float deltaTime)
{
    m_GameState->CurrentTimePrep -= (deltaTime / 1000.0f);
    if (m_GameState->CurrentTimePrep <= 0.0f)
    {
        m_GameState->currentState = Invasion;
    }
}

void GameRoundControllerSystem::SpawnCrystals()
{
    int curCrystalCount = ECS.Visit<CrystalDeposit>().size();
    float minDist = 10.0f;
    float maxDist = 20.0f;
    int amountToSpawn = AmountOfCrystalPresent - curCrystalCount;
    for (int i = 0; i < amountToSpawn; i++)
    {
        // Randomly spawn Crystal Randomly outside of a circle radius
        float angle = Rand01() * 2.0f * 3.1415926f;

        // Correct uniform distribution in a ring
        float r2 = minDist * minDist + Rand01() * (maxDist * maxDist - minDist * minDist);
        float radius = std::sqrt(r2);
        Vec3 spawnPos{};
        spawnPos.X = std::cos(angle) * radius;
        spawnPos.Z = std::sin(angle) * radius;
        CreateCrystal(spawnPos);
    }
    m_GameState->CurrentTimePrep = m_GameState->PrepPhaseTime;
    m_GameState->CurrentTimeInvasion = m_GameState->InvasionPhaseTime;
    m_GameState->InvasionPhaseTime += m_GameState->SpawnInterval;
    m_GameState->currentState = Prepartion;
}

void GameRoundControllerSystem::DeleteCrystals()
{
    for (Entity E : ECS.Visit<CrystalDeposit>())
    {
        CrystalDeposit& Cryst = ECS.GetComponent<CrystalDeposit>(E);
        if (Cryst.AmountOfCrystal <= 0)
            ECS.DestroyEntity(E);
    }
}
