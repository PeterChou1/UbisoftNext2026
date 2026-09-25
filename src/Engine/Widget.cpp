#include "Widget.h"

#include "Input.h"
#include "UIText.h"
#include "app.h"

namespace
{
    // Space kept free on both sides of a label inside its box
    constexpr float LABEL_PADDING = 3.0f;

    /// Filled rectangle drawn as horizontal lines 0.1 apart
    void DrawRect(float x, float y, float width, float height, float r, float g, float b)
    {
        for (float scanY = 0.0f; scanY <= height; scanY += 0.1f)
            App::DrawLine(x, y + scanY, x + width, y + scanY, r, g, b);
    }

    bool RegionHit(float mouseX, float mouseY, float x, float y, float width, float height)
    {
        return !(mouseX < x || mouseY < y || mouseX >= x + width || mouseY >= y + height);
    }

    /// Hovering the region makes the widget hot, clicking it makes it active
    void UpdateHotActive(int id, float x, float y, float width, float height, UIState& uiState)
    {
        if (RegionHit(uiState.mouseX, uiState.mouseY, x, y, width, height))
        {
            uiState.hotItem = id;
            if (uiState.leftClick)
                uiState.activeItem = id;
        }
    }

    bool Clicked(int id, const UIState& uiState)
    {
        return uiState.hotItem == id && uiState.activeItem == id;
    }

    /// Draw text centered in a box, shortened with ".." when it does not
    /// fit: labels stay inside their widget at any window size
    void PrintCentered(float x,
                       float y,
                       float width,
                       float height,
                       const std::string& text,
                       float r,
                       float g,
                       float b)
    {
        std::string shown = UIText::Fit(text, width - 2.0f * LABEL_PADDING);
        App::Print(UIText::CenterX(x, width, shown),
                   UIText::CenterY(y, height),
                   shown.c_str(),
                   r,
                   g,
                   b);
    }

    bool Accepts(TextFilter filter, char& c)
    {
        switch (filter)
        {
        case TextFilter::Number:
            return (c >= '0' && c <= '9') || c == '.' || c == '-';
        case TextFilter::Name:
            if (c == ' ')
                c = '_';
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   c == '_' || c == '-';
        case TextFilter::Any:
        default:
            return c >= 32 && c < 127;
        }
    }
} // namespace

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
    return Clicked(id, uiState) ? 1 : 0;
}

int Button(int id, float x, float y, UIState& uiState, float width, float height, std::string label)
{
    const float shadowOffsetX = 2.0f;

    UpdateHotActive(id, x, y, width, height, uiState);

    DrawRect(x - shadowOffsetX, y, width, height, 1.0f, 1.0f, 1.0f);
    if (Clicked(id, uiState))
    {
        App::PlayAudio("data/Sounds/clickSound.wav");
        DrawRect(x - shadowOffsetX, y, width, height, 0.8f, 0.8f, 0.8f);
    }
    else
    {
        const float shade = uiState.hotItem == id ? 0.7f : 0.6f;
        DrawRect(x, y, width, height, shade, shade, shade);
    }
    PrintCentered(x, y, width, height, label, 1.0f, 1.0f, 1.0f);

    return Clicked(id, uiState) ? 1 : 0;
}

int CheckBox(int id,
             float x,
             float y,
             bool state,
             float size,
             UIState& uiState,
             std::string label,
             float labelWidth)
{
    const float padding = 10.0f;

    UpdateHotActive(id, x, y, size, size, uiState);

    DrawRect(x, y, size, size, 1.0f, 1.0f, 1.0f);
    if (state)
        DrawRect(x + 1.0f, y + 1.0f, size - 2.0f, size - 2.0f, 1.0f, 0.0f, 0.0f);
    else
    {
        const float shade = uiState.hotItem == id ? 0.7f : 0.5f;
        DrawRect(x + 1.0f, y + 1.0f, size - 2.0f, size - 2.0f, shade, shade, shade);
    }

    if (!label.empty())
    {
        // Beside the box, vertically centered on it, cut to labelWidth
        std::string shown = labelWidth > 0.0f ? UIText::Fit(label, labelWidth) : label;
        App::Print(x + padding + size, UIText::CenterY(y, size), shown.c_str());
    }

    return Clicked(id, uiState) ? 1 : 0;
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
    const int itemCount = static_cast<int>(items.size());

    // The button shows the selected item, clicking it opens / closes the list
    UpdateHotActive(id, x, y, width, height, uiState);
    DrawRect(x, y, width, height, 0.6f, 0.6f, 0.6f);
    PrintCentered(x,
                  y,
                  width,
                  height,
                  currentIndex >= 0 && currentIndex < itemCount ? items[currentIndex] : "Select...",
                  1.0f,
                  1.0f,
                  1.0f);
    if (Clicked(id, uiState))
        uiState.openDropDownId = uiState.openDropDownId == id ? 0 : id;

    if (uiState.openDropDownId != id)
        return 0;

    // The items, one below the other under the button
    int changedSelection = 0;
    for (int i = 0; i < itemCount; i++)
    {
        const float itemY = y - height - i * height;
        const bool hover = RegionHit(uiState.mouseX, uiState.mouseY, x, itemY, width, height);
        if (hover)
        {
            uiState.hotItem = id;
            if (uiState.leftClick)
            {
                uiState.activeItem = id;
                currentIndex = i;
                changedSelection = 1;
                uiState.openDropDownId = 0;
            }
        }

        const float shade = hover ? 0.7f : 0.5f;
        DrawRect(x, itemY, width, height, shade, shade, shade);
        PrintCentered(x, itemY, width, height, items[i], 1.0f, 1.0f, 1.0f);
    }
    return changedSelection;
}

void FillBar(float x, float y, float width, float height, float fillVal)
{
    const float MAX_VALUE = 1000.0f;
    if (fillVal < 0.0f)
        fillVal = 0.0f;
    if (fillVal > MAX_VALUE)
        fillVal = MAX_VALUE;

    DrawRect(x, y, width, height, 0.2f, 0.2f, 0.2f);
    DrawRect(x, y, width * (fillVal / MAX_VALUE), height, 0.5f, 1.0f, 0.5f);
    PrintCentered(x,
                  y,
                  width,
                  height,
                  std::to_string(static_cast<int>(fillVal)) + " / 1000",
                  1.0f,
                  0.0f,
                  0.0f);
}

TextFieldEvent TextField(int id,
                         float x,
                         float y,
                         float width,
                         float height,
                         UIState& uiState,
                         std::string& text,
                         TextFilter filter,
                         size_t maxChars)
{
    bool hit = RegionHit(uiState.mouseX, uiState.mouseY, x, y, width, height);
    TextFieldEvent event = TextFieldEvent::None;

    // Another field took the focus before this one was drawn: keep the edit
    if (uiState.unfocusedItem == id)
    {
        text = uiState.unfocusedText;
        uiState.unfocusedItem = 0;
        event = TextFieldEvent::Committed;
    }

    bool editing = uiState.focusedItem == id;
    if (!editing && hit && uiState.leftClick)
    {
        if (uiState.focusedItem != 0)
        {
            // Hand the other field's text over so it commits when drawn
            uiState.unfocusedItem = uiState.focusedItem;
            uiState.unfocusedText = uiState.editText;
        }
        uiState.focusedItem = id;
        uiState.editText = text;
        uiState.editFresh = true;
        uiState.hotItem = id;
        uiState.activeItem = id;
        editing = true;
    }
    else if (editing)
    {
        if (uiState.leftClick && !hit)
        {
            text = uiState.editText;
            uiState.focusedItem = 0;
            event = TextFieldEvent::Committed;
            editing = false;
        }
        else
        {
            for (char c : Input::TypedText())
            {
                if (c == '\r' || c == '\n')
                {
                    text = uiState.editText;
                    uiState.focusedItem = 0;
                    event = TextFieldEvent::Committed;
                    editing = false;
                    break;
                }
                if (c == 27)
                {
                    uiState.focusedItem = 0;
                    event = TextFieldEvent::Cancelled;
                    editing = false;
                    break;
                }
                if (c == '\b' || c == 127)
                {
                    if (!uiState.editText.empty())
                        uiState.editText.pop_back();
                    uiState.editFresh = false;
                }
                else if (Accepts(filter, c))
                {
                    // Typing right after clicking replaces the old value
                    if (uiState.editFresh)
                        uiState.editText.clear();
                    uiState.editFresh = false;
                    if (uiState.editText.size() < maxChars)
                        uiState.editText.push_back(c);
                }
            }
        }
    }

    // Box, lighter while editing
    if (editing)
        DrawRect(x, y, width, height, 0.30f, 0.32f, 0.38f);
    else
        DrawRect(
                x, y, width, height, hit ? 0.24f : 0.18f, hit ? 0.25f : 0.19f, hit ? 0.29f : 0.22f);
    float border = editing ? 1.0f : 0.5f;
    App::DrawLine(x, y, x + width, y, border, border * 0.85f, border * 0.3f);
    App::DrawLine(x, y + height, x + width, y + height, border, border * 0.85f, border * 0.3f);

    // Long texts are cut to the box: the end while editing (where the
    // cursor is), the beginning otherwise
    float room = width - 8.0f;
    std::string shown =
            editing ? UIText::FitTail(uiState.editText + "_", room) : UIText::Fit(text, room);
    App::Print(x + 4.0f, UIText::CenterY(y, height), shown.c_str(), 1.0f, 1.0f, 1.0f);
    return event;
}
