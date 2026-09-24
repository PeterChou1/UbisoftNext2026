//---------------------------------------------------------------------------------
// AppStub.h
//---------------------------------------------------------------------------------
//
// Scriptable input / recorded output for the headless App API (stubs/app.h)
//
#pragma once

#include "app.h"

#include <string>
#include <vector>

namespace AppStub
{
    struct State
    {
        // Mouse in virtual screen coordinates with y UP (like UIState)
        float MouseX = 0.0f;
        float MouseY = 0.0f;
        bool LeftDown = false;
        bool RightDown = false;
        bool Keys[64] = {};
        struct PrintedText
        {
            float X, Y;
            std::string Text;
        };
        // Every string printed during the current frame, with its position
        std::vector<PrintedText> Printed;
        size_t LinesDrawn = 0;
    };

    State& Get();

    void Reset();

    /**
     * \brief True if some text containing `fragment` was printed this frame
     */
    bool WasPrinted(const std::string& fragment);

    /**
     * \brief First text printed this frame that starts with `prefix`
     *        (nullptr if none)
     */
    const State::PrintedText* FindPrinted(const std::string& prefix);
} // namespace AppStub
