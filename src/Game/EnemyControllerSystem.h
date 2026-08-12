#pragma once

#include "BlackBoard.h"
#include "Camera.h"
#include "GameState.h"
#include "UIState.h"

class EnemyControllerSystem
{
  public:
    EnemyControllerSystem();

    void Update(float deltaTime);

  private:
    void DeleteDeadEnemy();

    void SpawnEnemyUnits(float deltaTime);

    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<UIState> m_UIstate;
    std::shared_ptr<GameState> m_GameState;
    std::shared_ptr<BlackBoard> m_Board;
};
