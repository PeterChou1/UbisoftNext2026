//---------------------------------------------------------------------------------
// Widget.h
//---------------------------------------------------------------------------------
//
// Immediate mode UI widgets, drawn with ContestAPI lines and text. Each widget
// takes a unique id; the int returning ones return 1 when clicked (see
// UIState for hot / active items)
//
#pragma once
#include "UIState.h"

#include <string>
#include <vector>

struct Color
{
    float R, G, B;
    Color(float R, float G, float B)
        : R(R)
        , G(G)
        , B(B)
    {
    }
};

/// Bar filled to fillVal out of 1000, with "fillVal / 1000" on it
void FillBar(float x, float y, float width, float height, float fillVal);

/// Light gray rectangle with a dark gray one offset over it
void DrawContainer(int x, int y, float width, float height);

/// Filled rectangle with a border, cheaper than DrawContainer for large areas
/// (fills every other scanline)
void DrawPanel(float x, float y, float width, float height, Color fill, Color border);

/// Clickable colour square (outlined when selected)
int ColorSwatch(int id, float x, float y, float size, Color color, bool selected, UIState& uiState);

int Button(int id,
           float x,
           float y,
           UIState& uiState,
           float width = 100,
           float height = 100,
           std::string label = "Default");

/// Check box with a label on its right, cut to labelWidth when given
int CheckBox(int id,
             float x,
             float y,
             bool state,
             float size,
             UIState& uiState,
             std::string label = "",
             float labelWidth = 0.0f);

/// Button showing items[currentIndex] that opens the list of items below it.
/// Returns 1 when an item was picked (currentIndex is set to it)
int DropdownList(int id,
                 float x,
                 float y,
                 float width,
                 float height,
                 UIState& uiState,
                 const std::vector<std::string>& items,
                 int& currentIndex);

enum class TextFieldEvent
{
    None,
    Committed, // Enter or a click elsewhere: `text` holds the new value
    Cancelled  // Esc: `text` is unchanged
};

enum class TextFilter
{
    Any,
    Number, // digits, '.', '-'
    Name    // letters, digits, '_', '-' (space becomes '_')
};

/// Single line text box. Click it to edit, type, then Enter (or click
/// anywhere else) commits and Esc cancels. Shows `text` while not being
/// edited. Give each field a fixed id: the focus is kept by id across frames
/// (UIState::focusedItem)
TextFieldEvent TextField(int id,
                         float x,
                         float y,
                         float width,
                         float height,
                         UIState& uiState,
                         std::string& text,
                         TextFilter filter = TextFilter::Any,
                         size_t maxChars = 32);
