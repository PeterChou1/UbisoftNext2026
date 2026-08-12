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

int Button(int id,
           float x,
           float y,
           UIState& uiState,
           float width = 100,
           float height = 100,
           std::string label = "Default");

int CheckBox(
        int id, float x, float y, bool state, float size, UIState& uiState, std::string label = "");

int DropdownList(int id,
                 float x,
                 float y,
                 float width,
                 float height,
                 UIState& uiState,
                 const std::vector<std::string>& items,
                 int& currentIndex);