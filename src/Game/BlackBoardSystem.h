#pragma once
#include "BlackBoard.h"

#include <memory>

/// Sync the map with the BlackBoard and Updating the Vector Field
class BlackBoardSystem
{
  public:
    BlackBoardSystem();

    void Update(float deltaTime);

    void Render();

  private:
    std::shared_ptr<BlackBoard> Board;
    size_t PrevX, PrevY;
};
