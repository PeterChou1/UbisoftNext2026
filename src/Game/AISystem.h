#pragma once

#include "BlackBoard.h"

class AISystem
{
  public:
    AISystem();

    void Update();

  private:
    std::set<Entity> m_Obstacles;
    std::shared_ptr<BlackBoard> m_BlackBoard;
};
