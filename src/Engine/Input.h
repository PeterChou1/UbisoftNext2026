//---------------------------------------------------------------------------------
// Input.h
//---------------------------------------------------------------------------------
//
// Keyboard state with press / release detection, updated once per frame by
// the GameManager. Used by scripts (App::IsKeyPressed only reports "held")
//
#pragma once

#include "app.h"

#include <string>

namespace Input
{
    // Number of keys in App::Key
    constexpr int KEY_COUNT = static_cast<int>(App::KEY_INSERT) + 1;

    /**
     * \brief Sample the keyboard, call once at the start of every frame
     */
    void Update();

    /**
     * \brief Key is held down this frame
     */
    bool IsDown(App::Key key);

    /**
     * \brief Key went down this frame
     */
    bool WasPressed(App::Key key);

    /**
     * \brief Key went up this frame
     */
    bool WasReleased(App::Key key);

    /**
     * \brief Characters typed this frame (see App::GetTypedText), the same
     *        for every reader during the frame. Used by text fields
     */
    const std::string& TypedText();

    /**
     * \brief Forget every key (e.g. when a scene starts)
     */
    void Reset();
} // namespace Input
