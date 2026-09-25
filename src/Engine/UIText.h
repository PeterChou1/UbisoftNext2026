//---------------------------------------------------------------------------------
// UIText.h
//---------------------------------------------------------------------------------
//
// Text measurement for the immediate mode UI.
//
// The UI is laid out in virtual screen units (APP_VIRTUAL_WIDTH x
// APP_VIRTUAL_HEIGHT), which ContestAPI stretches over the window. Text is
// different: GLUT draws it in real pixels, so it keeps its size when the
// window grows or shrinks. Measured in virtual units, a label is therefore
// smaller in a large window and larger in a small one. Widgets use these
// functions so text stays centered in its box and never spills out of it,
// whatever the window size:
//
//     float w = UIText::Width("Save");               // virtual units
//     std::string s = UIText::Fit(name, 120.0f);     // "a very lo.."
//     App::Print(UIText::CenterX(x, width, s), UIText::CenterY(y, height), s.c_str());
//
// The programs tell it the window size every frame and how GLUT measures the
// font (UIText::SetWindowSize / SetMeasure, see EditorMain.cpp). Without a
// measure function (unit tests) it uses the advance widths of GLUT's
// Helvetica 18, the font App::Print draws with.
//
#pragma once

#include <functional>
#include <string>

namespace UIText
{
    /**
     * \brief Current window size in pixels (default: the virtual size)
     */
    void SetWindowSize(int width, int height);
    int WindowWidth();
    int WindowHeight();

    /**
     * \brief Pixel width of a string in the UI font (nullptr: built in table)
     */
    void SetMeasure(std::function<int(const std::string&)> measure);

    /**
     * \brief Width of the text in virtual units
     */
    float Width(const std::string& text);

    /**
     * \brief Height of capital letters in virtual units (what looks centered)
     */
    float CapHeight();

    /**
     * \brief The text if it fits in maxWidth, otherwise its beginning
     *        followed by ".." (empty when not even ".." fits)
     */
    std::string Fit(const std::string& text, float maxWidth);

    /**
     * \brief The end of the text that fits in maxWidth, with ".." in front
     *        (text boxes being edited show where the cursor is)
     */
    std::string FitTail(const std::string& text, float maxWidth);

    /**
     * \brief Left x that centers `text` in [x, x + width]
     */
    float CenterX(float x, float width, const std::string& text);

    /**
     * \brief Baseline y that centers a line of text in [y, y + height]
     */
    float CenterY(float y, float height);
} // namespace UIText
