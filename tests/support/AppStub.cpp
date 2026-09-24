#include "AppStub.h"

namespace AppStub
{
    State& Get()
    {
        static State state;
        return state;
    }

    void Reset()
    {
        Get() = State{};
    }

    bool WasPrinted(const std::string& fragment)
    {
        for (const auto& printed : Get().Printed)
        {
            if (printed.Text.find(fragment) != std::string::npos)
                return true;
        }
        return false;
    }

    const State::PrintedText* FindPrinted(const std::string& prefix)
    {
        for (const auto& printed : Get().Printed)
        {
            if (printed.Text.compare(0, prefix.size(), prefix) == 0)
                return &printed;
        }
        return nullptr;
    }
} // namespace AppStub

namespace App
{
    void DrawLine(const float,
                  const float,
                  const float,
                  const float,
                  const float,
                  const float,
                  const float)
    {
        ++AppStub::Get().LinesDrawn;
    }

    void Print(const float x,
               const float y,
               const char* text,
               const float,
               const float,
               const float,
               void*)
    {
        AppStub::Get().Printed.push_back({x, y, text});
    }

    void PlayAudio(const char*, const bool) {}

    bool IsKeyPressed(const Key key)
    {
        return AppStub::Get().Keys[static_cast<int>(key)];
    }

    void GetMousePos(float& x, float& y)
    {
        // The real API reports y DOWN, UIStateManager flips it
        x = AppStub::Get().MouseX;
        y = APP_VIRTUAL_HEIGHT - AppStub::Get().MouseY;
    }

    bool IsMousePressed(int button)
    {
        return button == GLUT_LEFT_BUTTON ? AppStub::Get().LeftDown : AppStub::Get().RightDown;
    }
} // namespace App
