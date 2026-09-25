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

void FillBar(float x, float y, float width, float height, float fillVal);

void TextLabel(float x, float y, float width, float height, std::string label, Color C, Color BG);

void DrawContainer(int x, int y, float width, float height);

/**
 * \brief Filled rectangle with a border, cheaper than DrawContainer for large
 *        areas (fills every other scanline)
 */
void DrawPanel(float x, float y, float width, float height, Color fill, Color border);

/**
 * \brief Clickable colour square (outlined when selected), returns 1 on click
 */
int ColorSwatch(int id, float x, float y, float size, Color color, bool selected, UIState& uiState);

int Button(int id,
           float x,
           float y,
           UIState& uiState,
           float width = 100,
           float height = 100,
           std::string label = "Default");

/**
 * \brief Check box with a label on its right, cut to labelWidth when given
 */
int CheckBox(int id,
             float x,
             float y,
             bool state,
             float size,
             UIState& uiState,
             std::string label = "",
             float labelWidth = 0.0f);

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

/**
 * \brief Single line text box. Click it to edit, type, then Enter (or click
 *        anywhere else) commits and Esc cancels. Shows `text` while not
 *        being edited. Give each field a fixed id: the focus is kept by id
 *        across frames (UIState::focusedItem)
 */
TextFieldEvent TextField(int id,
                         float x,
                         float y,
                         float width,
                         float height,
                         UIState& uiState,
                         std::string& text,
                         TextFilter filter = TextFilter::Any,
                         size_t maxChars = 32);
