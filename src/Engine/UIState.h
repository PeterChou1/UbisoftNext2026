#pragma once
#include "Resource.h"

enum UIContextState
{
    DefaultContext,
    InBaseContextMenu,
    BuildObstacleContext
};

class UIState : public Resource
{
  public:
    UIContextState state = DefaultContext;
    float mouseX;
    float mouseY;
    // whether mouse is held down
    bool mouseLeftDown = false;
    bool mouseRightDown = false;

    // whether mouse left is clicked
    bool leftClick = false;
    // whether mouse right is click
    bool rightClick = false;
    // whether the obstacle being interacted with is flipped
    bool flipped = false;

    // whether mouse is held down (previous frame)
    bool mouseLeftDownPrevFrame = false;
    bool mouseRightDownPrevFrame = false;

    // track if UI item is hot. an item is "hot" when the user is about to
    // interact with it e.g when a user hovers over a button
    int hotItem = -1;
    // track if UI item is active, an item is "active" when the user interact with
    // it e.g when a user
    int activeItem = -1;
    // id of the drop down list
    int openDropDownId = 0;

    void ResetResource() override {}
};
