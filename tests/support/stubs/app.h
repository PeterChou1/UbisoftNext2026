//---------------------------------------------------------------------------------
// app.h (test stub)
//---------------------------------------------------------------------------------
//
// Headless stand-in for the ContestAPI "app.h" (the real one pulls in
// OpenGL/GLUT). It declares the subset of the App API used by the engine code
// under test with the same signatures. AppStub.cpp implements it: input comes
// from a scriptable state and the frame presented by the renderer is captured,
// so the editor GUI and the render pipeline can be driven and checked by tests.
//
#pragma once
#include "AppSettings.h"

#define GLUT_LEFT_BUTTON 0
#define GLUT_RIGHT_BUTTON 2

namespace App
{
    // Same order as the real ContestAPI enum
    enum Key
    {
        KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, KEY_L, KEY_M,
        KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
        KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0,
        KEY_SPACE, KEY_ESC, KEY_ENTER, KEY_TAB, KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_HOME,
        KEY_END, KEY_INSERT,
    };

    // Gamepad (only the thumb sticks are used by engine code)
    class CController
    {
      public:
        float GetLeftThumbStickX() const { return 0.0f; }
        float GetLeftThumbStickY() const { return 0.0f; }
        float GetRightThumbStickX() const { return 0.0f; }
        float GetRightThumbStickY() const { return 0.0f; }
    };

    const CController& GetController(const int pad = 0);

    void DrawTriangle(const float p1x, const float p1y, const float p1z, const float p1w,
                      const float p2x, const float p2y, const float p2z, const float p2w,
                      const float p3x, const float p3y, const float p3z, const float p3w,
                      const float r1, const float g1, const float b1,
                      const float r2, const float g2, const float b2,
                      const float r3, const float g3, const float b3,
                      const bool isWireframe = false);

    void DrawLine(const float sx, const float sy, const float ex, const float ey,
                  const float r = 1.0f, const float g = 1.0f, const float b = 1.0f);
    void Print(const float x, const float y, const char* text, const float r = 1.0f,
               const float g = 1.0f, const float b = 1.0f, void* font = nullptr);
    void PlayAudio(const char* fileName, const bool isLooping = false);
    bool IsKeyPressed(const Key key);
    void GetMousePos(float& x, float& y);
    bool IsMousePressed(int button);
} // namespace App
