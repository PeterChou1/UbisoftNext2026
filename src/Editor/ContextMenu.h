//---------------------------------------------------------------------------------
// ContextMenu.h
//---------------------------------------------------------------------------------
//
// Right click menu of the scene editor: a list of items opened at the mouse,
// with submenus (items with children open on hover or click, to the right,
// or to the left near the screen edge).
//
//   m_Menu.Open(mouseX, mouseY, {
//       {"Duplicate", [this] { DuplicateSelected(); }},
//       {"Create Child", nullptr, {{"Empty", ...}, {"Circle", ...}}},
//   });
//   ...
//   m_Menu.Render(*m_UI);   // last in Render, drawn on top of everything
//
// Every level is sized from its labels and kept on the screen. A click on an
// item runs its action and closes the menu; a click anywhere else, a right
// click or Esc closes it. While it is open, the rest of the GUI gets no clicks.
//
#pragma once

#include <functional>
#include <string>
#include <vector>

class UIState;

struct MenuItem
{
    MenuItem(std::string label,
             std::function<void()> action = nullptr,
             std::vector<MenuItem> children = {},
             bool enabled = true)
        : Label(std::move(label))
        , Action(std::move(action))
        , Children(std::move(children))
        , Enabled(enabled)
    {
    }

    std::string Label;
    std::function<void()> Action;
    // Items with children open a submenu instead of running an action
    std::vector<MenuItem> Children;
    bool Enabled = true;
};

class ContextMenu
{
  public:
    static constexpr float ITEM_H = 22.0f;
    static constexpr float MIN_WIDTH = 120.0f;

    /**
     * \brief Show items with the top left corner at (x, y), moved to stay on
     *        the screen
     */
    void Open(float x, float y, std::vector<MenuItem> items);
    void Close();
    bool IsOpen() const { return !m_Items.empty(); }

    /**
     * \brief True over any open level of the menu
     */
    bool Contains(float x, float y) const;

    /**
     * \brief Draw the menu and handle this frame's mouse / Esc
     */
    void Render(UIState& ui);

    /**
     * \brief Labels of the open levels (tests)
     */
    std::vector<std::string> Labels() const;

  private:
    struct Level
    {
        const std::vector<MenuItem>* Items = nullptr;
        float X = 0.0f;
        float Top = 0.0f;
        float Width = 0.0f;
    };

    std::vector<Level> Levels() const;
    static float WidthOf(const std::vector<MenuItem>& items);

    std::vector<MenuItem> m_Items;
    float m_X = 0.0f;
    float m_Y = 0.0f;
    // Index of the open submenu at each level
    std::vector<int> m_OpenPath;
    // Opened this frame: the click that opened it is not an outside click
    bool m_JustOpened = false;
};
