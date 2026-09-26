#pragma once
#include "UIState.h"

#include <memory>

class UIStateManager
{
  public:
    UIStateManager();

    void Update();

    void CleanUp();

  private:
    std::shared_ptr<UIState> state;
};
