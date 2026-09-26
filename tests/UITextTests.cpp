//---------------------------------------------------------------------------------
// UITextTests.cpp
//---------------------------------------------------------------------------------
//
// Text measurement of the UI (UIText.h): widths follow the window size (text
// is drawn in pixels, the UI in virtual units), long text is cut with ".."
// to fit, and centering helpers
//
#include "UIText.h"
#include "WorldFixture.h"

#include <cmath>

namespace
{
    bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

    // Restores the default window size and measure
    struct WindowGuard
    {
        ~WindowGuard()
        {
            UIText::SetWindowSize(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT);
            UIText::SetMeasure(nullptr);
        }
    };
} // namespace

TEST_CASE("UI text: widths are measured in virtual units for the window size")
{
    WindowGuard guard;
    // The window is the virtual size: pixels = virtual units
    float save = UIText::Width("Save");
    CHECK(save > 30.0f && save < 50.0f);
    CHECK_EQ(UIText::Width(""), 0.0f);
    CHECK(UIText::Width("WWW") > UIText::Width("iii"));

    // A window twice as large: the same text takes half as many units
    UIText::SetWindowSize(APP_VIRTUAL_WIDTH * 2, APP_VIRTUAL_HEIGHT * 2);
    CHECK(Near(UIText::Width("Save"), save * 0.5f));
    float cap = UIText::CapHeight();
    UIText::SetWindowSize(APP_VIRTUAL_WIDTH / 2, APP_VIRTUAL_HEIGHT / 2);
    CHECK(Near(UIText::CapHeight(), cap * 4.0f));
    CHECK(Near(UIText::Width("Save"), save * 2.0f));
    // Degenerate sizes do not divide by zero
    UIText::SetWindowSize(0, 0);
    CHECK(std::isfinite(UIText::Width("Save")));

    // A program's own measure (GLUT's metrics) replaces the built in table
    UIText::SetWindowSize(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT);
    UIText::SetMeasure([](const std::string& text) { return static_cast<int>(text.size()) * 7; });
    CHECK_EQ(UIText::Width("abcd"), 28.0f);
}

TEST_CASE("UI text: long text is cut to fit, centering helpers")
{
    WindowGuard guard;
    UIText::SetMeasure([](const std::string& text) { return static_cast<int>(text.size()) * 10; });
    CHECK_EQ(UIText::Fit("Short", 100.0f), std::string("Short"));
    // The beginning, then ".." (20 units), within the room
    CHECK_EQ(UIText::Fit("A long label", 80.0f), std::string("A long.."));
    CHECK(UIText::Width(UIText::Fit("A long label", 80.0f)) <= 80.0f);
    // No space before the ".."
    CHECK_EQ(UIText::Fit("Hello world", 80.0f), std::string("Hello.."));
    // Not even ".." fits: nothing
    CHECK_EQ(UIText::Fit("Hello", 15.0f), std::string());
    // The end of a text being edited
    CHECK_EQ(UIText::FitTail("abcdefghij_", 60.0f), std::string("..hij_"));

    // Centered in a box
    CHECK_EQ(UIText::CenterX(100.0f, 60.0f, "abc"), 115.0f);
    UIText::SetWindowSize(APP_VIRTUAL_WIDTH, APP_VIRTUAL_HEIGHT);
    CHECK(Near(UIText::CenterY(10.0f, 22.0f), 10.0f + (22.0f - UIText::CapHeight()) * 0.5f));
}
