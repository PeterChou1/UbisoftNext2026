#include "EnemyControllerSystem.h"

#include "BasicEnemyUnit.h"
#include "ECSManager.h"
#include "PlayerBase.h"

extern ECSManager ECS;

EnemyControllerSystem::EnemyControllerSystem()
{
    m_Cam = ECS.GetResource<Camera>();
    m_UIstate = ECS.GetResource<UIState>();
    m_GameState = ECS.GetResource<GameState>();
    m_Board = ECS.GetResource<BlackBoard>();
}

void EnemyControllerSystem::Update(float deltaTime)
{
    DeleteDeadEnemy();
    if (m_GameState->CurrentTimeInvasion >= 0.0f)
        SpawnEnemyUnits(deltaTime);
}

void EnemyControllerSystem::DeleteDeadEnemy()
{
    for (Entity E : ECS.Visit<BasicEnemyUnit>())
    {
        BasicEnemyUnit& Enemy = ECS.GetComponent<BasicEnemyUnit>(E);
        if (Enemy.health < 0)
            ECS.DestroyEntity(E);
    }
}

void EnemyControllerSystem::SpawnEnemyUnits(float deltaTime)
{

    m_GameState->currentInterval -= (deltaTime / 1000);
    Entity Base = *ECS.Visit<PlayerBaseComponent>().begin();
    Transform& T = ECS.GetComponent<Transform>(Base);
    m_Board->EnemyVectorField.CalculateVectorField(T);

    if (m_GameState->currentInterval > 0.0)
        return;

    m_GameState->currentInterval = m_GameState->SpawnInterval;

    for (int i = 0; i < m_GameState->SpawnVolume; i++)
    {
        // Randomly spawn Enemy in a circle
        float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * PI;
        float spawnX = std::cos(angle) * m_GameState->spawnDistance;
        float spawnZ = std::sin(angle) * m_GameState->spawnDistance;
        float randSpeed =
                Utils::RandomFloat(m_GameState->enemySpeedLower, m_GameState->enemySpeedUpper);

        float roll = static_cast<float>(rand()) / RAND_MAX;
        // give 70 percent chance of spawning
        if (roll < m_GameState->EnemyToTankRatio)
        {
            // Spawn battalion
            float randSpeed =
                    Utils::RandomFloat(m_GameState->enemySpeedLower, m_GameState->enemySpeedUpper);
            CreateEnemyBattalion(spawnX, spawnZ, m_GameState->enemyHealth, 5, randSpeed);
        }
        else
        {
            // Spawn tank
            CreateEnemyTank(spawnX, spawnZ, m_GameState->enemyHealth);
        }
    }
}
