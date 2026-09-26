#include "ContextMenu.h"

#include "EditorStyle.h"
#include "Input.h"
#include "UIState.h"

using namespace EditorStyle;

namespace
{
    constexpr float PADDING = 10.0f;
    // Room for the ">" of items with a submenu
    constexpr float ARROW_W = 18.0f;
    const Color MENU_FILL = {0.16f, 0.17f, 0.21f};
    const Color MENU_BORDER = {0.62f, 0.64f, 0.70f};
    const Color MENU_HOVER = {0.30f, 0.33f, 0.42f};

    float Clamp(float v, float lo, float hi) { return std::max(lo, std::min(v, hi)); }
} // namespace

void ContextMenu::Open(float x, float y, std::vector<MenuItem> items)
{
    m_Items = std::move(items);
    m_X = x;
    m_Y = y;
    m_OpenPath.clear();
    m_JustOpened = true;
}

void ContextMenu::Close()
{
    m_Items.clear();
    m_OpenPath.clear();
}

float ContextMenu::WidthOf(const std::vector<MenuItem>& items)
{
    float width = MIN_WIDTH;
    for (const MenuItem& item : items)
        width = std::max(width, UIText::Width(item.Label) + 2.0f * PADDING + (item.Children.empty() ? 0.0f : ARROW_W));
    // Never wider than a third of the screen: long labels are cut instead
    return std::min(width, SCREEN_W / 3.0f);
}

std::vector<ContextMenu::Level> ContextMenu::Levels() const
{
    std::vector<Level> levels;
    if (m_Items.empty())
        return levels;
    // First level at the mouse, kept on the screen
    Level first;
    first.Items = &m_Items;
    first.Width = WidthOf(m_Items);
    float height = ITEM_H * static_cast<float>(m_Items.size());
    first.X = Clamp(m_X, 0.0f, SCREEN_W - first.Width);
    first.Top = Clamp(m_Y, height, SCREEN_H);
    levels.push_back(first);
    // Each open submenu next to its item: on the right, or on the left
    // when the right side of the screen is too close
    for (int index : m_OpenPath)
    {
        const Level& parent = levels.back();
        if (index < 0 || index >= static_cast<int>(parent.Items->size()))
            break;
        const MenuItem& item = (*parent.Items)[index];
        if (item.Children.empty())
            break;
        Level sub;
        sub.Items = &item.Children;
        sub.Width = WidthOf(item.Children);
        float subHeight = ITEM_H * static_cast<float>(item.Children.size());
        float right = parent.X + parent.Width;
        sub.X = right + sub.Width <= SCREEN_W ? right : std::max(0.0f, parent.X - sub.Width);
        sub.Top = Clamp(parent.Top - ITEM_H * static_cast<float>(index), subHeight, SCREEN_H);
        levels.push_back(sub);
    }
    return levels;
}

bool ContextMenu::Contains(float x, float y) const
{
    for (const Level& level : Levels())
    {
        float height = ITEM_H * static_cast<float>(level.Items->size());
        if (Inside(x, y, level.X, level.Top - height, level.Width, height))
            return true;
    }
    return false;
}

std::vector<std::string> ContextMenu::Labels() const
{
    std::vector<std::string> labels;
    for (const Level& level : Levels())
    {
        for (const MenuItem& item : *level.Items)
            labels.push_back(item.Label);
    }
    return labels;
}

void ContextMenu::Render(UIState& ui)
{
    if (!IsOpen())
        return;
    // Esc closes
    for (char c : Input::TypedText())
    {
        if (c == 27)
        {
            Close();
            return;
        }
    }
    // The click that opened the menu this frame is not for the menu
    bool acceptClicks = !m_JustOpened;
    m_JustOpened = false;

    // Hover first: the item under the mouse opens its submenu (or closes the
    // deeper ones), so what is drawn below is already up to date
    std::vector<Level> levels = Levels();
    for (std::size_t depth = 0; depth < levels.size(); ++depth)
    {
        const std::vector<MenuItem>& items = *levels[depth].Items;
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            float y = levels[depth].Top - ITEM_H * static_cast<float>(i + 1);
            if (!items[i].Enabled || !Inside(ui.mouseX, ui.mouseY, levels[depth].X, y, levels[depth].Width, ITEM_H))
                continue;
            m_OpenPath.resize(depth);
            if (!items[i].Children.empty())
                m_OpenPath.push_back(static_cast<int>(i));
            levels = Levels();
            break;
        }
    }

    bool clickedInside = false;
    std::function<void()> action;
    for (std::size_t depth = 0; depth < levels.size(); ++depth)
    {
        const Level& level = levels[depth];
        const std::vector<MenuItem>& items = *level.Items;
        float height = ITEM_H * static_cast<float>(items.size());
        DrawPanel(level.X, level.Top - height, level.Width, height, MENU_FILL, MENU_BORDER);
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            const MenuItem& item = items[i];
            float y = level.Top - ITEM_H * static_cast<float>(i + 1);
            bool hover = Inside(ui.mouseX, ui.mouseY, level.X, y, level.Width, ITEM_H);
            bool open = depth < m_OpenPath.size() && m_OpenPath[depth] == static_cast<int>(i);
            if ((hover && item.Enabled) || open)
                DrawPanel(level.X + 1.0f, y + 1.0f, level.Width - 2.0f, ITEM_H - 2.0f, MENU_HOVER, MENU_HOVER);
            const Color& color = item.Enabled ? TEXT : TEXT_DIM;
            float room = level.Width - 2.0f * PADDING - (item.Children.empty() ? 0.0f : ARROW_W);
            Text(level.X + PADDING, UIText::CenterY(y, ITEM_H), item.Label, color, room);
            if (!item.Children.empty())
                Text(level.X + level.Width - PADDING - UIText::Width(">"), UIText::CenterY(y, ITEM_H), ">", color, ARROW_W);
            // A click on an item runs it (items with children only open)
            if (hover && item.Enabled && ui.leftClick && acceptClicks)
            {
                clickedInside = true;
                if (item.Children.empty())
                    action = item.Action;
            }
        }
    }

    if (action)
    {
        // Close first: the action may open another menu
        Close();
        action();
        return;
    }
    if (!acceptClicks)
        return;
    bool outside = !clickedInside && !Contains(ui.mouseX, ui.mouseY);
    if ((ui.leftClick && outside) || ui.rightClick)
        Close();
}
