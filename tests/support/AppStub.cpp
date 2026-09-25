#include "AppStub.h"

namespace AppStub
{
    State& Get()
    {
        static State state;
        return state;
    }

    void Type(const std::string& text)
    {
        Get().Typed += text;
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

    Pixel PixelAt(int x, int y)
    {
        if (x < 0 || y < 0 || x >= APP_VIRTUAL_WIDTH || y >= APP_VIRTUAL_HEIGHT)
            return {0, 0, 0};
        const auto& frame = Get().Frame;
        size_t i = (static_cast<size_t>(y) * APP_VIRTUAL_WIDTH + x) * 3;
        return {frame[i], frame[i + 1], frame[i + 2]};
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
    void DrawLine(const float sx,
                  const float sy,
                  const float ex,
                  const float ey,
                  const float r,
                  const float g,
                  const float b)
    {
        auto& state = AppStub::Get();
        // The renderer presents every pixel as a 1 px diagonal line
        if (ex - sx == 1.0f && ey - sy == 1.0f)
        {
            int x = static_cast<int>(sx);
            int y = static_cast<int>(sy);
            if (x >= 0 && y >= 0 && x < APP_VIRTUAL_WIDTH && y < APP_VIRTUAL_HEIGHT)
            {
                size_t i = (static_cast<size_t>(y) * APP_VIRTUAL_WIDTH + x) * 3;
                state.Frame[i] = r;
                state.Frame[i + 1] = g;
                state.Frame[i + 2] = b;
            }
        }
        else
            state.Lines.push_back({sx, sy, ex, ey, r, g, b});
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

    std::string GetTypedText()
    {
        std::string text;
        text.swap(AppStub::Get().Typed);
        return text;
    }

    const CController& GetController(const int)
    {
        static CController controller;
        return controller;
    }

    void DrawTriangle(const float p1x,
                      const float p1y,
                      const float,
                      const float p1w,
                      const float p2x,
                      const float p2y,
                      const float,
                      const float p2w,
                      const float p3x,
                      const float p3y,
                      const float,
                      const float p3w,
                      const float r1,
                      const float g1,
                      const float b1,
                      const float r2,
                      const float g2,
                      const float b2,
                      const float r3,
                      const float g3,
                      const float b3,
                      const bool)
    {
        AppStub::State::DrawnTriangle t;
        t.Screen[0] = Vec2(p1x / p1w, p1y / p1w);
        t.Screen[1] = Vec2(p2x / p2w, p2y / p2w);
        t.Screen[2] = Vec2(p3x / p3w, p3y / p3w);
        t.Color[0] = Vec3(r1, g1, b1);
        t.Color[1] = Vec3(r2, g2, b2);
        t.Color[2] = Vec3(r3, g3, b3);
        AppStub::Get().Triangles.push_back(t);
    }
} // namespace App
