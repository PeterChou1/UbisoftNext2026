#include "Widget.h"

#include "UIUtilities.h"
#include "stdafx.h"

void TextLabel(float x, float y, float width, float height, std::string label, Color C, Color BG)
{
    DrawRect(x, y, width, height, BG.R, BG.G, BG.B);
    float textWidth = static_cast<float>(10 * label.length());
    float textHeight = 10.0f;
    // Calculate centered text position
    float textX = x + (width - textWidth) * 0.5f;
    float textY = y + (height - textHeight) * 0.5f;
    App::Print(textX, textY, label.c_str(), C.R, C.G, C.B);
}

void DrawContainer(int x, int y, float width, float height)
{
    float offset = 2.0f;
    DrawRect(x, y, width, height, 0.8f, 0.8f, 0.8f);
    DrawRect(x + offset, y + offset, width, height, 0.2f, 0.2f, 0.2f);
}

void DrawPanel(float x, float y, float width, float height, Color fill, Color border)
{
    for (float scanY = 0.0f; scanY <= height; scanY += 2.0f)
        App::DrawLine(x, y + scanY, x + width, y + scanY, fill.R, fill.G, fill.B);
    App::DrawLine(x, y, x + width, y, border.R, border.G, border.B);
    App::DrawLine(x + width, y, x + width, y + height, border.R, border.G, border.B);
    App::DrawLine(x + width, y + height, x, y + height, border.R, border.G, border.B);
    App::DrawLine(x, y + height, x, y, border.R, border.G, border.B);
}

int ColorSwatch(int id, float x, float y, float size, Color color, bool selected, UIState& uiState)
{
    bool hover = RegionHit(uiState.mouseX, uiState.mouseY, x, y, size, size);
    if (hover)
    {
        uiState.hotItem = id;
        if (uiState.leftClick)
            uiState.activeItem = id;
    }
    for (float scanY = 0.0f; scanY <= size; scanY += 1.0f)
        App::DrawLine(x, y + scanY, x + size, y + scanY, color.R, color.G, color.B);
    if (selected || hover)
    {
        float c = selected ? 1.0f : 0.6f;
        float o = 2.0f;
        App::DrawLine(x - o, y - o, x + size + o, y - o, c, c, c);
        App::DrawLine(x + size + o, y - o, x + size + o, y + size + o, c, c, c);
        App::DrawLine(x + size + o, y + size + o, x - o, y + size + o, c, c, c);
        App::DrawLine(x - o, y + size + o, x - o, y - o, c, c, c);
    }
    return uiState.hotItem == id && uiState.activeItem == id ? 1 : 0;
}

int Button(int id, float x, float y, UIState& uiState, float width, float height, std::string label)
{
    float shadowOffsetX = 2.0f;

    // Check for hover and click
    if (RegionHit(uiState.mouseX, uiState.mouseY, x, y, width, height))
    {
        uiState.hotItem = id;
        if (uiState.leftClick)
            uiState.activeItem = id;
    }

    // Draw the background shadow
    DrawRect(x - shadowOffsetX, y, width, height, 1.0f, 1.0f, 1.0f);

    float textWidth = 10.0f * label.length();
    float textHeight = 10.0f;
    // Calculate centered text position
    float textX = x + (width - textWidth) * 0.5f;
    float textY = y + (height - textHeight) * 0.5f;

    if (uiState.hotItem == id)
    {
        if (uiState.activeItem == id)
        {
            App::PlayAudio("data/Sounds/clickSound.wav");
            // Button is active (clicked)
            DrawRect(x - shadowOffsetX, y, width, height, 0.8f, 0.8f, 0.8f);
        }
        else
        {
            // Mouse is hovering over the button
            DrawRect(x, y, width, height, 0.7f, 0.7f, 0.7f);
        }

        App::Print(textX, textY, label.c_str(), 1.0f, 1.0f, 1.0f);
    }
    else
    {
        // Normal state
        DrawRect(x, y, width, height, 0.6f, 0.6f, 0.6f);
        App::Print(textX, textY, label.c_str(), 1.0f, 1.0f, 1.0f);
    }

    // Return 1 if the button was clicked
    if (uiState.hotItem == id && uiState.activeItem == id)
    {
        return 1;
    }

    return 0;
}

int CheckBox(int id, float x, float y, bool state, float size, UIState& uiState, std::string label)
{

    float padding = 10.0f;
    if (RegionHit(uiState.mouseX, uiState.mouseY, x, y, size, size))
    {
        uiState.hotItem = id;
        if (uiState.leftClick)
            uiState.activeItem = id;
    }

    DrawRect(x, y, size, size, 1.0, 1.0, 1.0);
    if (state)
    {
        DrawRect(x + 1.0f, y + 1.0f, size - 2.0f, size - 2.0f, 1.0, 0.0, 0.0);
    }
    else
    {
        if (uiState.hotItem == id)
        {
            DrawRect(x + 1.0f, y + 1.0f, size - 2.0f, size - 2.0f, 0.7f, 0.7f, 0.7f);
        }
        else
        {
            DrawRect(x + 1.0f, y + 1.0f, size - 2.0f, size - 2.0f, 0.5, 0.5, 0.5);
        }
    }

    if (label.length() > 0)
        App::Print(x + padding + size, y, label.c_str());

    if (uiState.hotItem == id && uiState.activeItem == id)
        return 1;

    return 0;
}

int DropdownList(int id,
                 float x,
                 float y,
                 float width,
                 float height,
                 UIState& uiState,
                 const std::vector<std::string>& items,
                 int& currentIndex)
{
    // --- 1. Draw the "main button" of the dropdown ---
    // Show the currently selected item as the label
    std::string selectedItemText = (currentIndex >= 0 && currentIndex < (int)items.size())
                                           ? items[currentIndex]
                                           : "Select...";

    // If the user hovers or clicks on the main rectangle, update hotItem/activeItem
    if (RegionHit(uiState.mouseX, uiState.mouseY, x, y, width, height))
    {
        uiState.hotItem = id;
        if (uiState.leftClick)
            uiState.activeItem = id;
    }

    // Draw a basic rectangle for the dropdown "button"
    DrawRect(x, y, width, height, 0.6f, 0.6f, 0.6f);

    // Draw the text label (centered)
    {
        float textWidth = 10.0f * selectedItemText.size();
        float textHeight = 10.0f;
        float textX = x + (width - textWidth) * 0.5f;
        float textY = y + (height - textHeight) * 0.5f;
        App::Print(textX, textY, selectedItemText.c_str(), 1.0f, 1.0f, 1.0f);
    }

    // If the user clicked on the dropdown button, toggle open/close
    bool clickedMainButton = (uiState.hotItem == id && uiState.activeItem == id);
    if (clickedMainButton)
    {
        if (uiState.openDropDownId == id)
            uiState.openDropDownId = 0; // close if it was open
        else
            uiState.openDropDownId = id; // open this dropdown
    }

    // --- 2. If not open, exit now ---
    if (uiState.openDropDownId != id)
    {
        // Return 0 => no selection changed
        return 0;
    }

    float itemHeight = height;
    float dropdownX = x;
    float dropdownY = y - height; // directly below main button

    // Track if we changed selection to return at the end
    int changedSelection = 0;

    for (int i = 0; i < (int)items.size(); i++)
    {
        // Region for each item
        float itemX = dropdownX;
        float itemY = dropdownY - i * itemHeight;
        float itemW = width;
        float itemH = itemHeight;

        // Is mouse over this item?
        bool hover = RegionHit(uiState.mouseX, uiState.mouseY, itemX, itemY, itemW, itemH);

        // If hover and clicked, we select this item
        if (hover)
        {
            uiState.hotItem = id; // This is still the same dropdown’s ID
            if (uiState.leftClick)
            {
                uiState.activeItem = id;
                // Set the new selection
                currentIndex = i;
                changedSelection = 1; // means a selection changed
                // Close the dropdown
                uiState.openDropDownId = 0;
            }
        }

        // Draw the item
        if (hover)
            DrawRect(itemX, itemY, itemW, itemH, 0.7f, 0.7f, 0.7f);
        else
            DrawRect(itemX, itemY, itemW, itemH, 0.5f, 0.5f, 0.5f);

        // Print the item text
        float textWidth = 10.0f * items[i].size();
        float textHeight = 10.0f;
        float textX = itemX + (itemW - textWidth) * 0.5f;
        float textY = itemY + (itemH - textHeight) * 0.5f;
        App::Print(textX, textY, items[i].c_str(), 1.0f, 1.0f, 1.0f);
    }

    return changedSelection;
}

void FillBar(float x, float y, float width, float height, float fillVal)
{
    // 1) Clamp the incoming health value between 0 and 100
    if (fillVal < 0.0f)
        fillVal = 0.0f;
    if (fillVal > 1000.0f)
        fillVal = 1000.0f;

    // 2) Draw a background rectangle (for empty portion)
    //    Let's make it a dark gray
    DrawRect(x, y, width, height, 0.2f, 0.2f, 0.2f);

    // 3) Calculate how much of the bar should be filled
    float fillRatio = fillVal / 1000.0f;
    float fillWidth = width * fillRatio;

    // 4) Draw the filled portion of the bar
    DrawRect(x, y, fillWidth, height, 0.5f, 1.0f, 0.5f);

    // 5) Display the health text on top, e.g. "75 / 100"
    {
        // Build string
        std::string fillText = std::to_string(static_cast<int>(fillVal)) + " / 1000";

        // Calculate roughly how wide the text is, based on your widget code
        float textWidth = 10.0f * fillText.size();
        float textHeight = 10.0f;

        // Center the text inside the bar
        float textX = x + (width - textWidth) * 0.5f;
        float textY = y + (height - textHeight) * 0.5f;

        // Draw the text in white. You can also use TextLabel(...) if you prefer
        App::Print(textX, textY, fillText.c_str(), 1.0f, 0.0f, 0.0f);
    }
}