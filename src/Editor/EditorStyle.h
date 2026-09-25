//---------------------------------------------------------------------------------
// EditorStyle.h
//---------------------------------------------------------------------------------
//
// Layout, colours, widget ids and small drawing helpers shared by the scene
// editor's GUI files (SceneEditorScene.cpp, EditorLeftPanel.cpp,
// EditorInspector.cpp, ContextMenu.cpp).
//
//   +-------------------------------------------------------------------------+
//   | [scene v] New Revert Save Undo Redo  Play  Scene    Name [my_level] *    |  toolbar
//   +------------+---------------------------------------------+--------------+
//   | HIERARCHY +|                                             | INSPECTOR   ||
//   |  tree      |        the scene (3D renderer)              |  components ||  scrollbar
//   |------------|   right click: context menu                 |  sections   ||
//   | ASSETS     |                                             |  Add Comp.  ||
//   +------------+---------------------------------------------+--------------+
//   | status line / tooltips / key hints                                      |
//   +-------------------------------------------------------------------------+
//
// All coordinates are virtual screen units (APP_VIRTUAL_WIDTH x
// APP_VIRTUAL_HEIGHT, y up). Text is measured with UIText so it stays
// centered and inside its widget whatever the window size.
//
#pragma once

#include "UIText.h"
#include "Vec3.h"
#include "Widget.h"
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace EditorStyle
{
    // -- Layout ---------------------------------------------------------------------
    constexpr float SCREEN_W = static_cast<float>(APP_VIRTUAL_WIDTH);
    constexpr float SCREEN_H = static_cast<float>(APP_VIRTUAL_HEIGHT);
    constexpr float TOOLBAR_H = 50.0f;
    constexpr float STATUS_H = 50.0f;
    constexpr float LEFT_W = 200.0f;
    constexpr float INSPECTOR_W = 270.0f;
    constexpr float BUTTON_H = 26.0f;
    constexpr float ROW_H = 26.0f;
    constexpr float WIDGET_H = 22.0f;
    constexpr float STEP_W = 24.0f;
    constexpr float SWATCH = 17.0f;
    constexpr float SCROLLBAR_W = 10.0f;
    // Width of the labels in front of typed values
    constexpr float LABEL_W = 70.0f;
    // Top of the panels (below the toolbar) and bottom (above the status bar)
    constexpr float PANEL_TOP = SCREEN_H - TOOLBAR_H;
    constexpr float PANEL_BOTTOM = STATUS_H;

    // -- Colours --------------------------------------------------------------------
    const Color PANEL_FILL = {0.12f, 0.13f, 0.16f};
    const Color PANEL_BORDER = {0.45f, 0.47f, 0.52f};
    const Color SECTION_FILL = {0.18f, 0.19f, 0.23f};
    const Color TEXT = {0.92f, 0.92f, 0.92f};
    const Color TEXT_DIM = {0.62f, 0.64f, 0.68f};
    const Color ACCENT = {1.0f, 0.82f, 0.25f};
    const Color ERROR_TEXT = {1.0f, 0.4f, 0.35f};
    const Color PLAY_TEXT = {0.4f, 1.0f, 0.5f};
    const Color PREFAB_TEXT = {0.45f, 0.75f, 1.0f};
    const Color ROW_SELECTED = {0.30f, 0.27f, 0.12f};
    const Color ROW_TARGET = {0.16f, 0.30f, 0.20f};
    const Color ROW_HOVER = {0.22f, 0.24f, 0.30f};
    // Empties (crosses) and the selection's hierarchy links in the viewport
    const Color EMPTY_COLOR = {0.55f, 0.85f, 1.0f};
    // Game camera objects: gizmo in the viewport, name in the hierarchy
    const Color CAMERA_COLOR = {1.0f, 0.85f, 0.35f};
    // The light object: sun marker, rays to the ground, hierarchy row
    const Color LIGHT_COLOR = {1.0f, 0.6f, 0.2f};
    constexpr float LIGHT_GIZMO_SIZE = 0.8f;
    constexpr float CAMERA_GIZMO_SIZE = 1.2f;
    const Color PARENT_LINK = {1.0f, 0.55f, 0.25f};
    const Color CHILD_LINK = {0.45f, 0.75f, 1.0f};

    const Vec3 COLORS[] = {{0.90f, 0.30f, 0.25f},
                           {0.95f, 0.60f, 0.20f},
                           {0.95f, 0.85f, 0.30f},
                           {0.35f, 0.75f, 0.35f},
                           {0.25f, 0.70f, 0.70f},
                           {0.30f, 0.50f, 0.90f},
                           {0.60f, 0.40f, 0.85f},
                           {0.85f, 0.85f, 0.85f}};
    constexpr int COLOR_COUNT = sizeof(COLORS) / sizeof(COLORS[0]);

    const char* const TAGS[] = {"", "Player", "Enemy", "Pickup", "Wall", "Goal", "Hazard", "Spawner", "Camera", "Light"};
    constexpr int TAG_COUNT = sizeof(TAGS) / sizeof(TAGS[0]);

    // -- Editing steps ----------------------------------------------------------------
    constexpr float SNAP_STEP = 0.5f;
    constexpr float ROTATE_STEP = 15.0f;
    constexpr float SIZE_STEP = 0.25f;

    // -- Widget ids -----------------------------------------------------------------
    // Widgets that keep state across frames have fixed ids: the scene list
    // (open / closed) and the text fields (being edited). Buttons use dynamic
    // ids from FIRST_DYNAMIC_ID
    constexpr int ID_SCENE_LIST = 1;
    constexpr int ID_FIELD_SCENE_NAME = 2;
    constexpr int ID_FIELD_IMPORT = 3;
    // Fields of the selected object (dropped when the selection changes)
    constexpr int ID_FIELD_FIRST_OBJECT = 10;
    constexpr int ID_FIELD_NAME = 10;
    constexpr int ID_FIELD_POS_X = 11;
    constexpr int ID_FIELD_POS_Z = 12;
    constexpr int ID_FIELD_ROT = 13;
    constexpr int ID_FIELD_WIDTH = 14;
    constexpr int ID_FIELD_HEIGHT = 15;
    constexpr int ID_FIELD_SIDES = 16;
    constexpr int ID_FIELD_THICK = 17;
    constexpr int ID_FIELD_SCALE = 18;
    constexpr int ID_FIELD_PARENT = 19;
    constexpr int ID_FIELD_POS_Y = 30;
    constexpr int ID_FIELD_PARAM = 20;
    constexpr int ID_FIELD_LAST_OBJECT = 39;
    // Scene settings fields
    constexpr int ID_FIELD_FIELD_W = 40;
    constexpr int ID_FIELD_FIELD_H = 41;
    constexpr int ID_FIELD_SCENE_PARAM = 42;
    constexpr int MAX_PARAM_FIELDS = 8;
    // Text fields of reflected component fields (in drawing order)
    constexpr int ID_FIELD_FIRST_COMPONENT = 100;
    constexpr int ID_FIELD_LAST_COMPONENT = 699;
    constexpr int FIRST_DYNAMIC_ID = 1000;

    // -- Helpers --------------------------------------------------------------------

    inline std::string Fmt(const char* format, float value)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), format, value);
        return buffer;
    }

    /**
     * \brief Right edge of the panel a point is in: text starting there is cut
     *        to it, so it never spills into the next panel
     */
    inline float PanelRight(float x, float y)
    {
        constexpr float MARGIN = 4.0f;
        if (y < STATUS_H || y > SCREEN_H - TOOLBAR_H || x >= SCREEN_W - INSPECTOR_W)
            return SCREEN_W - MARGIN;
        if (x < LEFT_W)
            return LEFT_W - MARGIN;
        return SCREEN_W - INSPECTOR_W - MARGIN;
    }

    /**
     * \brief Print text cut to maxWidth (0: to the end of its panel) with ".."
     */
    inline void Text(float x, float y, const std::string& text, const Color& c = TEXT, float maxWidth = 0.0f)
    {
        float room = maxWidth > 0.0f ? maxWidth : PanelRight(x, y) - x;
        std::string shown = UIText::Fit(text, room);
        App::Print(x, y, shown.c_str(), c.R, c.G, c.B);
    }

    // Baseline that centers text in a widget row (WIDGET_H high) at y
    inline float RowY(float y) { return UIText::CenterY(y, WIDGET_H); }

    inline Color ToColor(const Vec3& v) { return Color(v.X, v.Y, v.Z); }

    inline bool SameColor(const Vec3& a, const Vec3& b)
    {
        return std::fabs(a.X - b.X) < 0.01f && std::fabs(a.Y - b.Y) < 0.01f && std::fabs(a.Z - b.Z) < 0.01f;
    }

    // Cycle an index through [0, count) by delta
    inline int Cycle(int index, int delta, int count)
    {
        if (count <= 0)
            return 0;
        return ((index + delta) % count + count) % count;
    }

    template <typename List, typename T>
    int IndexOf(const List& list, const T& value)
    {
        auto it = std::find(std::begin(list), std::end(list), value);
        return it == std::end(list) ? 0 : static_cast<int>(it - std::begin(list));
    }

    inline std::string TagLabel(const std::string& tag) { return tag.empty() ? "-" : tag; }

    // [""] + names, used by the script pickers
    inline std::vector<std::string> WithNone(std::vector<std::string> names)
    {
        names.insert(names.begin(), "");
        return names;
    }

    inline bool Inside(float px, float py, float x, float y, float w, float h)
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
} // namespace EditorStyle
