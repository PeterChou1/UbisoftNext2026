//---------------------------------------------------------------------------------
// EditorLeftPanel.cpp
//---------------------------------------------------------------------------------
//
// Left panel of the scene editor:
//
//   HIERARCHY                  [+]     "+" or a right click: create objects
//   SCENE   12 objects                 drop a row here: top level
//     Field
//   - Tank                             drag a row onto another: its child
//       Hull                           right click a row: its context menu
//   ----------------------------------
//   ASSETS            [New Prefab]
//   Prefabs                            click, then click the scene: place it
//     turret                           (or drag it onto the scene)
//   Models
//     Box
//   [ import .obj  ] [Import]
//
// Both lists scroll (scrollbar on the right) when they are longer than the
// panel.
//
#include "EditorStyle.h"
#include "SceneEditorScene.h"
#include "UIState.h"

#include <cmath>

using namespace EditorStyle;

namespace
{
    constexpr float ASSETS_H = 240.0f;
    constexpr float TREE_ROW_H = 20.0f;
    constexpr float TREE_INDENT = 12.0f;
    constexpr float FOLD_W = 18.0f;
    // Mouse travel that turns a press on a row into a drag
    constexpr float TREE_DRAG_START = 5.0f;
    constexpr int SCROLL_TREE = 2;
    constexpr int SCROLL_ASSETS = 3;
} // namespace

void SceneEditorScene::RenderLeftPanel()
{
    if (m_Editor.IsPlaying())
        return;
    DrawPanel(0, PANEL_BOTTOM, LEFT_W, PANEL_TOP - PANEL_BOTTOM, PANEL_FILL, PANEL_BORDER);
    float split = PANEL_BOTTOM + ASSETS_H;
    App::DrawLine(0, split, LEFT_W, split, PANEL_BORDER.R, PANEL_BORDER.G, PANEL_BORDER.B);
    RenderHierarchy(PANEL_TOP, split);
    RenderAssets(split, PANEL_BOTTOM);
}

//-----------------------------------------------------------------------------
// Hierarchy
//-----------------------------------------------------------------------------

std::vector<std::pair<Entity, int>> SceneEditorScene::HierarchyRows() const
{
    std::vector<std::pair<Entity, int>> rows;
    // Depth first: each object, then (unless collapsed) its children
    std::vector<std::pair<Entity, int>> stack;
    std::vector<Entity> roots = m_Editor.RootObjects();
    for (auto it = roots.rbegin(); it != roots.rend(); ++it)
        stack.emplace_back(*it, 0);
    while (!stack.empty())
    {
        auto [e, depth] = stack.back();
        stack.pop_back();
        rows.emplace_back(e, depth);
        if (m_Collapsed.count(e) != 0)
            continue;
        std::vector<Entity> children = m_Editor.ChildrenOf(e);
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            stack.emplace_back(*it, depth + 1);
    }
    return rows;
}

void SceneEditorScene::RenderHierarchy(float top, float bottom)
{
    float x = 10.0f;
    float width = LEFT_W - 20.0f;
    float mx = m_UI->mouseX;
    float my = m_UI->mouseY;

    // Title and the "+" create button
    float y = top - 28.0f;
    std::string title = m_PrefabMode ? "PREFAB " + m_PrefabName : std::string("HIERARCHY");
    Text(x, RowY(y), title, m_PrefabMode ? PREFAB_TEXT : ACCENT, width - 30.0f);
    if (Button(NextId(), x + width - WIDGET_H, y, *m_UI, WIDGET_H, WIDGET_H, "+"))
        m_Menu.Open(x + width - WIDGET_H, y, CreateItems(ViewCenter(), NULL_ENTITY));

    // Forget folds of objects that no longer exist
    for (auto it = m_Collapsed.begin(); it != m_Collapsed.end();)
        it = m_Editor.IsObject(*it) ? std::next(it) : m_Collapsed.erase(it);

    std::vector<std::pair<Entity, int>> rows = HierarchyRows();

    // A newly selected object is revealed: its parents unfold, the tree
    // scrolls to it
    Entity selected = m_Editor.Selected();
    bool reveal = selected != m_TreeSelected && selected != NULL_ENTITY;
    m_TreeSelected = selected;
    if (reveal)
    {
        bool unfolded = false;
        for (Entity p = m_Editor.ParentOf(selected); p != NULL_ENTITY; p = m_Editor.ParentOf(p))
            unfolded = m_Collapsed.erase(p) != 0 || unfolded;
        if (unfolded)
            rows = HierarchyRows();
    }

    // SCENE row: drop a row here to make it a top level object
    float headerBottom = y - 4.0f - TREE_ROW_H;
    bool overHeader = Inside(mx, my, x, headerBottom, width, TREE_ROW_H);
    if (m_TreeDragging && overHeader)
        DrawPanel(x - 2.0f, headerBottom, width + 4.0f, TREE_ROW_H, ROW_TARGET, ROW_TARGET);
    float headerText = UIText::CenterY(headerBottom, TREE_ROW_H);
    Text(x, headerText, "SCENE", ACCENT, 76.0f);
    Text(x + 80.0f,
         headerText,
         std::to_string(m_Editor.Objects().size()) + " objects",
         TEXT_DIM,
         width - 80.0f);

    // The rows, scrolled
    float listTop = headerBottom - 2.0f;
    float listBottom = bottom + 6.0f;
    float view = listTop - listBottom;
    float content = TREE_ROW_H * static_cast<float>(rows.size());
    bool scrolls = content > view;
    float rowW = scrolls ? width - SCROLLBAR_W - 4.0f : width;
    if (reveal)
    {
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            if (rows[i].first != selected)
                continue;
            float rowTop = TREE_ROW_H * static_cast<float>(i);
            if (rowTop < m_TreeScroll)
                m_TreeScroll = rowTop;
            else if (rowTop + TREE_ROW_H > m_TreeScroll + view)
                m_TreeScroll = rowTop + TREE_ROW_H - view;
        }
    }
    m_TreeScroll = std::clamp(m_TreeScroll, 0.0f, std::max(0.0f, content - view));

    Entity hovered = NULL_ENTITY;
    bool overList = Inside(mx, my, x, listBottom, width, view);
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        auto [e, depth] = rows[i];
        float rowBottom = listTop + m_TreeScroll - TREE_ROW_H * static_cast<float>(i + 1);
        // Only rows fully inside the list are drawn
        if (rowBottom + TREE_ROW_H > listTop + 0.5f || rowBottom < listBottom - 0.5f)
            continue;
        float indent = x + static_cast<float>(depth) * TREE_INDENT;
        bool over = Inside(mx, my, x, rowBottom, rowW, TREE_ROW_H);
        if (over)
            hovered = e;
        if (e == selected)
            DrawPanel(x - 2.0f, rowBottom, rowW + 4.0f, TREE_ROW_H, ROW_SELECTED, ROW_SELECTED);
        else if (m_TreeDragging && over && e != m_TreePressed)
            DrawPanel(x - 2.0f, rowBottom, rowW + 4.0f, TREE_ROW_H, ROW_TARGET, ROW_TARGET);

        // Fold toggle of an object with children
        bool hasChildren = !m_Editor.ChildrenOf(e).empty();
        bool folded = m_Collapsed.count(e) != 0;
        bool overFold = hasChildren && mx >= indent && mx <= indent + FOLD_W;
        if (hasChildren &&
            Button(NextId(), indent, rowBottom + 2.0f, *m_UI, FOLD_W, 16.0f, folded ? "+" : "-"))
        {
            if (folded)
                m_Collapsed.erase(e);
            else
                m_Collapsed.insert(e);
        }
        std::string name = m_Editor.IsField(e) ? std::string("Field") : m_Editor.NameOf(e);
        // Prefab instances are blue, like in Unity; game cameras yellow, lights orange
        const Color& color = m_Editor.IsField(e)             ? TEXT_DIM
                             : !m_Editor.PrefabOf(e).empty() ? PREFAB_TEXT
                             : m_Editor.IsCamera(e)          ? CAMERA_COLOR
                             : m_Editor.IsLight(e)           ? LIGHT_COLOR
                                                             : TEXT;
        float textX = indent + FOLD_W + 4.0f;
        Text(textX, UIText::CenterY(rowBottom, TREE_ROW_H), name, color, x + rowW - textX);

        if (!over || overFold || m_UI->IsTyping())
            continue;
        if (m_UI->leftClick)
        {
            // Press: select (and maybe start dragging it onto another row)
            m_Editor.Select(e);
            m_TreeSelected = e;
            m_TreePressed = e;
            m_TreeDragging = false;
            m_TreePressX = mx;
            m_TreePressY = my;
        }
        else if (m_UI->rightClick)
        {
            m_Editor.Select(e);
            m_TreeSelected = e;
            m_Menu.Open(mx, my, ObjectItems(e));
        }
    }
    // Right click on the empty part of the tree (or on SCENE): create
    if (m_UI->rightClick && !m_Menu.IsOpen() &&
        ((overList && hovered == NULL_ENTITY) || overHeader))
        m_Menu.Open(mx, my, CreateItems(ViewCenter(), NULL_ENTITY));

    Scrollbar(SCROLL_TREE, x + width - SCROLLBAR_W, listBottom, listTop, content, m_TreeScroll);

    // Drag and drop: parent the pressed object to the row it is dropped on
    if (m_TreePressed == NULL_ENTITY)
        return;
    if (m_UI->mouseLeftDown)
    {
        if (std::fabs(mx - m_TreePressX) > TREE_DRAG_START ||
            std::fabs(my - m_TreePressY) > TREE_DRAG_START)
            m_TreeDragging = !m_Editor.IsField(m_TreePressed);
        if (m_TreeDragging)
        {
            m_Hint = "Drop " + m_Editor.NameOf(m_TreePressed) +
                     " on an object to make it its child, on SCENE for the top level";
            Text(mx + 12.0f, my - 4.0f, m_Editor.NameOf(m_TreePressed), ACCENT);
        }
        return;
    }
    // Released
    Entity dragged = m_TreePressed;
    bool dropped = m_TreeDragging;
    m_TreePressed = NULL_ENTITY;
    m_TreeDragging = false;
    if (!dropped)
        return;
    if (overHeader)
    {
        if (m_Editor.ParentOf(dragged) == NULL_ENTITY)
            return;
        if (m_Editor.SetParent(dragged, NULL_ENTITY))
            SetStatus(m_Editor.NameOf(dragged) + " is now a top level object");
        return;
    }
    if (hovered == NULL_ENTITY || hovered == dragged)
        return;
    if (m_Editor.SetParent(dragged, hovered))
    {
        m_Collapsed.erase(hovered);
        SetStatus(m_Editor.NameOf(dragged) + " is now a child of " + m_Editor.NameOf(hovered));
    }
    else if (m_Editor.IsField(dragged) || m_Editor.IsField(hovered))
        SetStatus("The field can not be a parent or a child", true);
    else
        SetStatus("Can not put " + m_Editor.NameOf(dragged) + " under " + m_Editor.NameOf(hovered) +
                          " (it is one of its children)",
                  true);
}

//-----------------------------------------------------------------------------
// Assets
//-----------------------------------------------------------------------------

void SceneEditorScene::RenderAssets(float top, float bottom)
{
    float x = 10.0f;
    float width = LEFT_W - 20.0f;
    float mx = m_UI->mouseX;
    float my = m_UI->mouseY;

    float y = top - 28.0f;
    constexpr float TITLE_W = 74.0f;
    Text(x, RowY(y), "ASSETS", ACCENT, TITLE_W - 2.0f);
    if (!m_PrefabMode &&
        Button(NextId(), x + TITLE_W, y, *m_UI, width - TITLE_W, WIDGET_H, "New Prefab"))
        NewPrefab();

    // Import box at the bottom: a path, or a file name in data/import/
    float importY = bottom + 6.0f;
    bool imported = false;
    float importW = width - 64.0f;
    if (TextField(ID_FIELD_IMPORT,
                  x,
                  importY,
                  importW,
                  WIDGET_H,
                  *m_UI,
                  m_ImportPath,
                  TextFilter::Any,
                  260) == TextFieldEvent::Committed)
    {
        ImportModel();
        imported = true;
    }
    if (m_ImportPath.empty() && m_UI->focusedItem != ID_FIELD_IMPORT)
        Text(x + 4.0f, RowY(importY), "import .obj", TEXT_DIM, importW - 8.0f);
    if (Button(NextId(), x + importW + 4.0f, importY, *m_UI, 60.0f, WIDGET_H, "Import") &&
        !imported)
        ImportModel();

    // Prefabs then models, scrolled
    struct Entry
    {
        std::string Name;
        AssetKind Kind;
    };
    std::vector<Entry> entries;
    entries.push_back({"Prefabs", AssetKind::None});
    for (const std::string& name : m_Prefabs)
        entries.push_back({name, AssetKind::Prefab});
    entries.push_back({"Models", AssetKind::None});
    for (const std::string& name : m_Models)
        entries.push_back({name, AssetKind::Model});

    float listTop = y - 4.0f;
    float listBottom = importY + WIDGET_H + 6.0f;
    float view = listTop - listBottom;
    float content = TREE_ROW_H * static_cast<float>(entries.size());
    bool scrolls = content > view;
    float rowW = scrolls ? width - SCROLLBAR_W - 4.0f : width;
    m_AssetScroll = std::clamp(m_AssetScroll, 0.0f, std::max(0.0f, content - view));

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        const Entry& entry = entries[i];
        float rowBottom = listTop + m_AssetScroll - TREE_ROW_H * static_cast<float>(i + 1);
        if (rowBottom + TREE_ROW_H > listTop + 0.5f || rowBottom < listBottom - 0.5f)
            continue;
        float textY = UIText::CenterY(rowBottom, TREE_ROW_H);
        if (entry.Kind == AssetKind::None)
        {
            Text(x, textY, entry.Name, TEXT_DIM, rowW);
            continue;
        }
        bool placing = m_PlaceKind == entry.Kind && m_PlaceName == entry.Name;
        bool over = Inside(mx, my, x, rowBottom, rowW, TREE_ROW_H);
        if (placing)
            DrawPanel(x - 2.0f, rowBottom, rowW + 4.0f, TREE_ROW_H, ROW_SELECTED, ROW_SELECTED);
        else if (over)
            DrawPanel(x - 2.0f, rowBottom, rowW + 4.0f, TREE_ROW_H, ROW_HOVER, ROW_HOVER);
        Text(x + 12.0f,
             textY,
             entry.Name,
             entry.Kind == AssetKind::Prefab ? PREFAB_TEXT : TEXT,
             rowW - 12.0f);
        if (!over || m_UI->IsTyping())
            continue;
        m_Hint = "Click " + entry.Name +
                 ", then click the scene to place it (or drag it onto the scene)";
        if (m_UI->leftClick)
        {
            StartPlacing(entry.Kind, entry.Name);
            // Keep the button down and drag it onto the scene to drop it there
            m_AssetDrag = true;
        }
        else if (m_UI->rightClick)
        {
            std::string name = entry.Name;
            AssetKind kind = entry.Kind;
            std::vector<MenuItem> items;
            items.push_back({"Place at View Center", [this, name, kind] {
                                 StartPlacing(kind, name);
                                 PlaceAsset(ViewCenter());
                                 StopPlacing();
                             }});
            if (kind == AssetKind::Prefab)
            {
                MenuItem edit{"Edit Prefab", [this, name] { EditPrefab(name); }};
                edit.Enabled = !m_PrefabMode;
                items.push_back(edit);
            }
            m_Menu.Open(mx, my, items);
        }
    }
    Scrollbar(SCROLL_ASSETS, x + width - SCROLLBAR_W, listBottom, listTop, content, m_AssetScroll);
}
