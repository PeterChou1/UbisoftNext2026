#pragma once
#include "Resource.h"

#include <string>

class UIState : public Resource
{
  public:
    float mouseX;
    float mouseY;
    // whether mouse is held down
    bool mouseLeftDown = false;
    bool mouseRightDown = false;

    // whether mouse left is clicked
    bool leftClick = false;
    // whether mouse right is click
    bool rightClick = false;

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

    // Text field being edited (0 = none) and its text so far
    int focusedItem = 0;
    std::string editText;
    // Just focused: the first typed character replaces the whole text
    bool editFresh = false;
    // A text field that lost the focus to another one this frame commits
    // this text when it is next drawn (see TextField)
    int unfocusedItem = 0;
    std::string unfocusedText;

    bool IsTyping() const { return focusedItem != 0; }

    void ResetResource() override {}
};
