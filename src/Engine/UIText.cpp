#include "UIText.h"

#include "app.h"

#include <algorithm>

namespace UIText
{
    namespace
    {
        int g_WindowWidth = APP_VIRTUAL_WIDTH;
        int g_WindowHeight = APP_VIRTUAL_HEIGHT;
        std::function<int(const std::string&)> g_Measure;

        // Advance widths (pixels) of GLUT_BITMAP_HELVETICA_18 for ASCII 32..126
        const unsigned char HELVETICA_18[95] = {
                5,  5,  6,  10, 10, 16, 12, 4,  6,  6,  7,  11, 5,  6,  5,  5,  // ' ' .. '/'
                10, 10, 10, 10, 10, 10, 10, 10, 10, 10,                         // '0' .. '9'
                5,  5,  11, 11, 11, 10, 18,                                     // ':' .. '@'
                12, 12, 13, 13, 12, 11, 14, 13, 5,  9,  12, 10, 15,             // 'A' .. 'M'
                13, 14, 12, 14, 13, 12, 11, 13, 12, 17, 12, 12, 11,             // 'N' .. 'Z'
                5,  5,  5,  8,  10, 4,                                          // '[' .. '`'
                10, 10, 9,  10, 10, 5,  10, 10, 4,  4,  9,  4,  15,             // 'a' .. 'm'
                10, 10, 10, 10, 6,  9,  5,  10, 9,  13, 9,  9,  9,              // 'n' .. 'z'
                6,  5,  6,  11                                                  // '{' .. '~'
        };
        // Capital letters of Helvetica 18 are 13 pixels high
        constexpr float CAP_PIXELS = 13.0f;
        const char* const ELLIPSIS = "..";

        int TablePixels(const std::string& text)
        {
            int pixels = 0;
            for (unsigned char c : text)
                pixels += c >= 32 && c <= 126 ? HELVETICA_18[c - 32] : 10;
            return pixels;
        }
    } // namespace

    void SetWindowSize(int width, int height)
    {
        g_WindowWidth = std::max(width, 1);
        g_WindowHeight = std::max(height, 1);
    }

    void SetMeasure(std::function<int(const std::string&)> measure) { g_Measure = std::move(measure); }

    float Width(const std::string& text)
    {
        int pixels = g_Measure ? g_Measure(text) : TablePixels(text);
        return static_cast<float>(pixels) * APP_VIRTUAL_WIDTH / static_cast<float>(g_WindowWidth);
    }

    float CapHeight() { return CAP_PIXELS * APP_VIRTUAL_HEIGHT / static_cast<float>(g_WindowHeight); }

    std::string Fit(const std::string& text, float maxWidth)
    {
        if (Width(text) <= maxWidth)
            return text;
        std::string cut = text;
        while (!cut.empty() && Width(cut + ELLIPSIS) > maxWidth)
            cut.pop_back();
        // Don't end on a space before the ellipsis
        while (!cut.empty() && cut.back() == ' ')
            cut.pop_back();
        std::string fitted = cut + ELLIPSIS;
        return Width(fitted) <= maxWidth ? fitted : std::string();
    }

    std::string FitTail(const std::string& text, float maxWidth)
    {
        if (Width(text) <= maxWidth)
            return text;
        std::string cut = text;
        while (!cut.empty() && Width(ELLIPSIS + cut) > maxWidth)
            cut.erase(cut.begin());
        std::string fitted = ELLIPSIS + cut;
        return Width(fitted) <= maxWidth ? fitted : std::string();
    }

    float CenterX(float x, float width, const std::string& text) { return x + (width - Width(text)) * 0.5f; }

    float CenterY(float y, float height) { return y + (height - CapHeight()) * 0.5f; }
} // namespace UIText
