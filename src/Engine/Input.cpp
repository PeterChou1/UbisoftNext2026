#include "Input.h"

namespace Input
{
    namespace
    {
        bool g_Down[KEY_COUNT] = {};
        bool g_Previous[KEY_COUNT] = {};
    } // namespace

    void Update()
    {
        for (int i = 0; i < KEY_COUNT; ++i)
        {
            g_Previous[i] = g_Down[i];
            g_Down[i] = App::IsKeyPressed(static_cast<App::Key>(i));
        }
    }

    bool IsDown(App::Key key) { return g_Down[key]; }

    bool WasPressed(App::Key key) { return g_Down[key] && !g_Previous[key]; }

    bool WasReleased(App::Key key) { return !g_Down[key] && g_Previous[key]; }

    void Reset()
    {
        for (int i = 0; i < KEY_COUNT; ++i)
        {
            g_Down[i] = false;
            g_Previous[i] = false;
        }
    }
} // namespace Input
