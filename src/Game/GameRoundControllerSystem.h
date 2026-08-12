#pragma once

#include "EnemyControllerSystem.h"

class GameRoundControllerSystem
{
  public:
    GameRoundControllerSystem();

    void Update(float deltaTime);

  private:
    void CheckGameOver();

    void InvadeEnemy(float deltaTime);

    void Preparation(float deltaTime);

    void SpawnCrystals();

    void DeleteCrystals();

    int AmountOfCrystalPresent = 10;
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<EnemyControllerSystem> m_EnemyController;
};
