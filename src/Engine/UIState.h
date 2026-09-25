#pragma once
#include "Resource.h"

#include <string>

class UIState : public Resource
{
  public:
    // Mouse position in virtual screen units (y up)
    float mouseX;
    float mouseY;
    // Mouse buttons held down
    bool mouseLeftDown = false;
    bool mouseRightDown = false;

    // Mouse button went down this frame
    bool leftClick = false;
    bool rightClick = false;

    // Mouse buttons held down in the previous frame
    bool mouseLeftDownPrevFrame = false;
    bool mouseRightDownPrevFrame = false;

    // Widget the user is about to interact with (hovered), -1: none
    int hotItem = -1;
    // Widget the user interacts with (clicked this frame), -1: none
    int activeItem = -1;
    // Open drop down list (0: none)
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
