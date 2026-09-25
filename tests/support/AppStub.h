//---------------------------------------------------------------------------------
// AppStub.h
//---------------------------------------------------------------------------------
//
// Scriptable input / recorded output for the headless App API (stubs/app.h)
//
#pragma once

#include "Vec2.h"
#include "Vec3.h"
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
        // Characters "typed" but not read yet (App::GetTypedText)
        std::string Typed;
        struct PrintedText
        {
            float X, Y;
            std::string Text;
        };
        // Every string printed during the current frame, with its position
        std::vector<PrintedText> Printed;
        size_t LinesDrawn = 0;
        // Lines drawn this frame by the UI / overlays (not the renderer's
        // 1 px pixel lines)
        struct DrawnLine
        {
            float X1, Y1, X2, Y2, R, G, B;
        };
        std::vector<DrawnLine> Lines;
        struct DrawnTriangle
        {
            Vec2 Screen[3]; // NDC x, y of each corner (after the w divide)
            Vec3 Color[3];
        };
        // Triangles sent to the hardware path (GameOptions::LineRendering)
        std::vector<DrawnTriangle> Triangles;

        // Last frame presented by the renderer (FragmentShaderSystem draws every
        // pixel as a 1 px line), RGB in [0, 1], index y * width + x
        std::vector<float> Frame = std::vector<float>(APP_VIRTUAL_WIDTH * APP_VIRTUAL_HEIGHT * 3, 0.0f);
    };

    State& Get();

    /**
     * \brief Queue typed characters ('\b' backspace, '\r' enter, 27 escape)
     */
    void Type(const std::string& text);

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

    struct Pixel
    {
        float R, G, B;
    };

    /**
     * \brief Colour of a pixel of the last presented frame (y up)
     */
    Pixel PixelAt(int x, int y);
} // namespace AppStub
