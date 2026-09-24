#include "SceneEditorScene.h"

#include "../Camera.h"
#include "../CreateMainLevel.h"
#include "../ECSManager.h"
#include "../GameManager.h"
#include "../GameOptions.h"
#include "../GameState.h"
#include "../IndexBuffer.h"
#include "../Lighting.h"
#include "../RenderConstants.h"
#include "../UIState.h"
#include "../VertexBuffer.h"
#include "../Widget.h"
#include "../stdafx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

extern ECSManager ECS;
extern GameManager GameSceneManager;

using Editor::EntityKind;
using Editor::PrefabType;

namespace
{
    // -- Layout (virtual screen coordinates, y up) ---------------------------------
    constexpr float SCREEN_W = static_cast<float>(APP_VIRTUAL_WIDTH);
    constexpr float SCREEN_H = static_cast<float>(APP_VIRTUAL_HEIGHT);
    constexpr float TOOLBAR_H = 50.0f;
    constexpr float STATUS_H = 50.0f;
    constexpr float PALETTE_W = 170.0f;
    constexpr float INSPECTOR_W = 230.0f;
    constexpr float BUTTON_H = 26.0f;
    constexpr float LINE_H = 24.0f;
    constexpr float STEP_BUTTON_W = 26.0f;

    const Color PANEL_FILL = {0.12f, 0.13f, 0.16f};
    const Color PANEL_BORDER = {0.45f, 0.47f, 0.52f};
    const Color TEXT = {0.92f, 0.92f, 0.92f};
    const Color TEXT_DIM = {0.62f, 0.64f, 0.68f};
    const Color ACCENT = {1.0f, 0.82f, 0.25f};
    const Color ERROR_TEXT = {1.0f, 0.4f, 0.35f};

    // -- Camera -------------------------------------------------------------------------
    constexpr float PAN_SPEED = 18.0f;  // world units per second
    constexpr float ZOOM_SPEED = 25.0f; // world units per second
    constexpr float MIN_DISTANCE = 12.0f;
    constexpr float MAX_DISTANCE = 60.0f;
    const Vec3 VIEW_DIRECTION = Vec3(0.0f, 1.0f, -0.55f).Normalize();

    constexpr float SNAP_STEP = 0.5f;
    constexpr float ROTATE_STEP = 45.0f;
    constexpr float STATUS_TIME = 4.0f; // seconds
    constexpr float DRAG_THRESHOLD = 0.01f;

    // -- Widget ids (unique per frame) -----------------------------------------------
    enum WidgetId
    {
        ID_SCENE_LIST = 100,
        ID_NEW,
        ID_LOAD,
        ID_SAVE,
        ID_UNDO,
        ID_REDO,
        ID_PLAY,
        ID_MENU,
        ID_TOOL_SELECT = 200,
        ID_PREFAB_FIRST = 210,
        ID_SNAP = 250,
        ID_BRUSH_STEPPERS = 300,
        ID_INSPECTOR_STEPPERS = 400,
        ID_ROTATE = 480,
        ID_DELETE,
        ID_SCENE_STEPPERS = 500,
    };

    // Scene slots offered in addition to the files already in data/scenes
    const char* const DEFAULT_SLOTS[] = {"my_scene_1", "my_scene_2", "my_scene_3"};

    std::string Format(const char* format, float value)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), format, value);
        return buffer;
    }

    void Text(float x, float y, const std::string& text, const Color& c = TEXT)
    {
        App::Print(x, y, text.c_str(), c.R, c.G, c.B);
    }
} // namespace

//-----------------------------------------------------------------------------
// Scene lifetime
//-----------------------------------------------------------------------------

void SceneEditorScene::Start()
{
    m_Cam = ECS.GetResource<Camera>();
    m_Light = ECS.GetResource<Lighting>();
    m_Options = ECS.GetResource<GameOptions>();
    m_UI = ECS.GetResource<UIState>();
    m_Editor.OnWorldReplaced = [this] {
        ResetRenderCaches();
        m_Dragging = false;
    };
}

void SceneEditorScene::Setup()
{
    m_Light->SetLightPerspective(120.0f, m_Options->ScreenRatio, 0.1f, 1000.0f);
    m_Light->SetPositionAndTarget(Vec3(0.0f, 25.0f, -5.0f), Vec3(0.0f, 0.0f, 0.0f));
    LoadMainLevelAssets();
    m_Cam->SetProjectionPerspective();

    m_CamTarget = Vec3(0, 0, 0);
    m_CamDistance = 34.0f;
    m_Tool = Tool::Select;
    m_Dragging = false;
    m_UI->openDropDownId = 0;

    RefreshSceneList();
    if (m_ResumeFromPlaytest)
    {
        m_ResumeFromPlaytest = false;
        if (m_Editor.LoadScene(PLAYTEST_PATH))
        {
            SetStatus("Back from the play test, the scene is unchanged");
            return;
        }
    }
    m_Editor.NewScene();
    SetStatus("New scene. Pick a prefab on the left and click the ground to place it");
}

void SceneEditorScene::Update(float deltaTime)
{
    float deltaSeconds = deltaTime / 1000.0f;
    if (m_StatusTimer > 0.0f)
        m_StatusTimer -= deltaSeconds;

    UpdateCamera(deltaSeconds);
    UpdateShortcuts();
    UpdateViewportMouse();
}

void SceneEditorScene::Render()
{
    // While the scene list is open it covers other widgets: they must not
    // receive the click meant for a list entry
    bool listOpen = m_UI->openDropDownId == ID_SCENE_LIST;
    bool click = m_UI->leftClick;
    if (listOpen)
        m_UI->leftClick = false;

    RenderOverlay();
    RenderPalette();
    RenderInspector();
    RenderStatusBar();
    if (!RenderToolbar())
        return; // switched scene

    // Last so the list draws on top of everything
    m_UI->leftClick = click;
    DropdownList(ID_SCENE_LIST,
                 10.0f,
                 SCREEN_H - TOOLBAR_H + (TOOLBAR_H - BUTTON_H) * 0.5f,
                 210.0f,
                 BUTTON_H,
                 *m_UI,
                 m_SceneNames,
                 m_SceneIndex);
    // A click anywhere else closes the list
    if (listOpen && click && m_UI->openDropDownId == ID_SCENE_LIST &&
        m_UI->activeItem != ID_SCENE_LIST)
        m_UI->openDropDownId = 0;
}

//-----------------------------------------------------------------------------
// Input
//-----------------------------------------------------------------------------

bool SceneEditorScene::KeyPressed(App::Key key)
{
    bool down = App::IsKeyPressed(key);
    bool& held = m_KeyHeld[static_cast<int>(key)];
    bool pressed = down && !held;
    held = down;
    return pressed;
}

void SceneEditorScene::UpdateCamera(float deltaSeconds)
{
    Vec3 pan(0, 0, 0);
    if (App::IsKeyPressed(App::KEY_W))
        pan.Z += 1.0f;
    if (App::IsKeyPressed(App::KEY_S))
        pan.Z -= 1.0f;
    if (App::IsKeyPressed(App::KEY_A))
        pan.X += 1.0f;
    if (App::IsKeyPressed(App::KEY_D))
        pan.X -= 1.0f;
    // Pan faster when zoomed out
    float speed = PAN_SPEED * (m_CamDistance / 34.0f) * deltaSeconds;
    m_CamTarget = Editor::SceneEditor::ClampToPlayArea(m_CamTarget + pan * speed);

    if (App::IsKeyPressed(App::KEY_Z))
        m_CamDistance -= ZOOM_SPEED * deltaSeconds;
    if (App::IsKeyPressed(App::KEY_C))
        m_CamDistance += ZOOM_SPEED * deltaSeconds;
    m_CamDistance = std::max(MIN_DISTANCE, std::min(MAX_DISTANCE, m_CamDistance));

    m_Cam->SetPositionAndOrientation(
            m_CamTarget + VIEW_DIRECTION * m_CamDistance, m_CamTarget, {0, 1, 0});
}

void SceneEditorScene::UpdateShortcuts()
{
    // Number keys pick a prefab, space goes back to the select tool
    const App::Key prefabKeys[] = {
            App::KEY_1, App::KEY_2, App::KEY_3, App::KEY_4, App::KEY_5, App::KEY_6, App::KEY_7};
    for (int i = 0; i < static_cast<int>(PrefabType::Count); ++i)
    {
        if (KeyPressed(prefabKeys[i]))
            SelectPrefab(static_cast<PrefabType>(i));
    }
    if (KeyPressed(App::KEY_SPACE))
        m_Tool = Tool::Select;

    if (KeyPressed(App::KEY_R))
        RotateSelected(ROTATE_STEP);
    if (KeyPressed(App::KEY_X))
        DeleteSelected();
    if (KeyPressed(App::KEY_U) && m_Editor.Undo())
        SetStatus("Undo");
    if (KeyPressed(App::KEY_Y) && m_Editor.Redo())
        SetStatus("Redo");
    if (KeyPressed(App::KEY_G))
        m_Snap = !m_Snap;
}

bool SceneEditorScene::MouseOverUI() const
{
    float x = m_UI->mouseX;
    float y = m_UI->mouseY;
    // An open dropdown list covers part of the viewport
    return m_UI->openDropDownId != 0 || x < PALETTE_W || x > SCREEN_W - INSPECTOR_W ||
           y > SCREEN_H - TOOLBAR_H || y < STATUS_H;
}

bool SceneEditorScene::MouseToGround(Vec3& groundPoint) const
{
    Vec3 planePoint(0, 0, 0);
    Vec3 planeNormal(0, 1, 0);
    groundPoint =
            m_Cam->ScreenSpaceToWorldPoint(m_UI->mouseX, m_UI->mouseY, planePoint, planeNormal);
    return groundPoint.IsValid();
}

Vec3 SceneEditorScene::SnapPoint(const Vec3& point) const
{
    if (!m_Snap)
        return point;
    return {Editor::SceneEditor::Snap(point.X, SNAP_STEP),
            point.Y,
            Editor::SceneEditor::Snap(point.Z, SNAP_STEP)};
}

void SceneEditorScene::UpdateViewportMouse()
{
    // Finish a drag as soon as the button is released, wherever the mouse is
    if (m_Dragging && !m_UI->mouseLeftDown)
    {
        m_Dragging = false;
        if (m_DragRecorded)
            SetStatus("Moved " +
                      std::string(Editor::KindName(m_Editor.KindOf(m_Editor.Selected()))));
    }

    Vec3 ground;
    if (!MouseToGround(ground))
        return;

    if (m_Dragging)
    {
        Entity selected = m_Editor.Selected();
        Vec3 target = SnapPoint(ground + m_DragOffset);
        Vec3 delta = target - m_Editor.GetPosition(selected);
        if (std::fabs(delta.X) > DRAG_THRESHOLD || std::fabs(delta.Z) > DRAG_THRESHOLD)
        {
            // Record a single undo step for the whole drag, only once it moves
            if (!m_DragRecorded)
            {
                m_Editor.RecordUndo();
                m_DragRecorded = true;
            }
            m_Editor.Move(selected, target, false);
        }
        return;
    }

    if (MouseOverUI())
        return;

    if (m_UI->rightClick)
    {
        m_Tool = Tool::Select;
        m_Editor.Select(NULL_ENTITY);
        return;
    }
    if (!m_UI->leftClick)
        return;

    if (m_Tool == Tool::Place)
    {
        Entity placed = m_Editor.Place(m_Prefab, SnapPoint(ground), m_Brush);
        SetStatus(std::string("Placed ") + Editor::PrefabName(m_Prefab) + " (entity " +
                  std::to_string(placed) + ")");
        return;
    }

    Entity picked = m_Editor.Pick(ground);
    m_Editor.Select(picked);
    if (picked != NULL_ENTITY)
    {
        m_Dragging = true;
        m_DragRecorded = false;
        m_DragStart = m_Editor.GetPosition(picked);
        m_DragOffset = m_DragStart - ground;
        m_DragOffset.Y = 0.0f;
    }
}

//-----------------------------------------------------------------------------
// Actions
//-----------------------------------------------------------------------------

void SceneEditorScene::SelectPrefab(PrefabType type)
{
    m_Prefab = type;
    m_Tool = Tool::Place;
    SetStatus(std::string("Placing ") + Editor::PrefabName(type) +
              ". Click the ground, right click to stop");
}

void SceneEditorScene::DeleteSelected()
{
    Entity selected = m_Editor.Selected();
    if (selected == NULL_ENTITY)
        return;
    std::string name = Editor::KindName(m_Editor.KindOf(selected));
    if (m_Editor.Remove(selected))
        SetStatus("Deleted " + name);
    else
        SetStatus("The " + name + " is required by the game and can not be deleted", true);
}

void SceneEditorScene::RotateSelected(float degrees)
{
    Entity selected = m_Editor.Selected();
    if (selected == NULL_ENTITY)
        return;
    // Walls only support two orientations, a 90 degree step toggles them
    float step = m_Editor.KindOf(selected) == EntityKind::Wall ? 90.0f : degrees;
    m_Editor.SetYaw(selected, m_Editor.GetYaw(selected) + step);
}

void SceneEditorScene::NewScene()
{
    m_Editor.NewScene();
    SetStatus("New scene");
}

void SceneEditorScene::SaveScene()
{
    Serialization::SaveResult result = m_Editor.SaveScene(CurrentScenePath(), CurrentSceneName());
    if (result)
    {
        SetStatus("Saved " + CurrentScenePath() + " (" + std::to_string(result.BytesWritten) +
                  " bytes)");
        std::string current = CurrentSceneName();
        RefreshSceneList();
        auto it = std::find(m_SceneNames.begin(), m_SceneNames.end(), current);
        m_SceneIndex = it == m_SceneNames.end() ? 0 : static_cast<int>(it - m_SceneNames.begin());
    }
    else
        SetStatus("Save failed: " + result.Error, true);
}

void SceneEditorScene::LoadScene()
{
    Serialization::LoadResult result = m_Editor.LoadScene(CurrentScenePath());
    if (result)
        SetStatus("Loaded " + CurrentScenePath());
    else
        SetStatus("Load failed: " + result.Error, true);
}

void SceneEditorScene::PlayScene()
{
    Serialization::SaveResult result = m_Editor.SaveScene(PLAYTEST_PATH, CurrentSceneName());
    if (!result)
    {
        SetStatus("Can not play: " + result.Error, true);
        return;
    }
    // Loaded at the start of the next frame into the main level (with AI);
    // TAB in the main level comes back here
    m_ResumeFromPlaytest = true;
    GameSceneManager.BeginPlaytest(PLAYTEST_PATH, "SceneEditor");
}

void SceneEditorScene::RefreshSceneList()
{
    m_SceneNames.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(SCENES_DIRECTORY, ec))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".ubsave")
            m_SceneNames.push_back(entry.path().stem().string());
    }
    for (const char* slot : DEFAULT_SLOTS)
    {
        if (std::find(m_SceneNames.begin(), m_SceneNames.end(), slot) == m_SceneNames.end())
            m_SceneNames.push_back(slot);
    }
    std::sort(m_SceneNames.begin(), m_SceneNames.end());
    m_SceneIndex = std::min(m_SceneIndex, static_cast<int>(m_SceneNames.size()) - 1);
}

std::string SceneEditorScene::CurrentSceneName() const
{
    if (m_SceneIndex < 0 || m_SceneIndex >= static_cast<int>(m_SceneNames.size()))
        return DEFAULT_SLOTS[0];
    return m_SceneNames[m_SceneIndex];
}

std::string SceneEditorScene::CurrentScenePath() const
{
    return std::string(SCENES_DIRECTORY) + "/" + CurrentSceneName() + ".ubsave";
}

void SceneEditorScene::ResetRenderCaches()
{
    // The world was replaced without per entity delete events, drop every
    // per entity render cache so the MeshHandler / ShaderHandler rebuild them
    ECS.GetResource<VertexBuffer>()->ResetResource();
    ECS.GetResource<IndexBuffer>()->ResetResource();
    auto constants = ECS.GetResource<RenderConstants>();
    constants->ResetResource();
    constants->EntityToFragShaderID.clear();
    constants->EntityToVertShaderID.clear();
    constants->EntityToFragShaderType.clear();
    constants->EntityToVertShaderType.clear();
}

void SceneEditorScene::SetStatus(const std::string& message, bool error)
{
    m_Status = message;
    m_StatusIsError = error;
    m_StatusTimer = STATUS_TIME;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

bool SceneEditorScene::Stepper(
        int id, float x, float y, float width, const std::string& label, int& delta)
{
    // "label   [-] [+]" row, delta is -1 / +1 when a button was clicked
    Text(x, y + 7.0f, label);
    float right = x + width;
    delta = 0;
    if (Button(id, right - 2 * STEP_BUTTON_W - 4.0f, y, *m_UI, STEP_BUTTON_W, 22.0f, "-"))
        delta = -1;
    if (Button(id + 1, right - STEP_BUTTON_W, y, *m_UI, STEP_BUTTON_W, 22.0f, "+"))
        delta = 1;
    return delta != 0;
}

bool SceneEditorScene::RenderToolbar()
{
    float y = SCREEN_H - TOOLBAR_H;
    DrawPanel(0, y, SCREEN_W, TOOLBAR_H, PANEL_FILL, PANEL_BORDER);
    float by = y + (TOOLBAR_H - BUTTON_H) * 0.5f;

    struct ToolbarButton
    {
        int Id;
        const char* Label;
        float Width;
    };
    const ToolbarButton buttons[] = {{ID_NEW, "New", 60},
                                     {ID_LOAD, "Load", 60},
                                     {ID_SAVE, "Save", 60},
                                     {ID_UNDO, "Undo", 60},
                                     {ID_REDO, "Redo", 60},
                                     {ID_PLAY, "Play", 60},
                                     {ID_MENU, "Menu", 60}};
    float x = 230.0f;
    for (const auto& b : buttons)
    {
        if (Button(b.Id, x, by, *m_UI, b.Width, BUTTON_H, b.Label))
        {
            switch (b.Id)
            {
            case ID_NEW:
                NewScene();
                break;
            case ID_LOAD:
                LoadScene();
                break;
            case ID_SAVE:
                SaveScene();
                break;
            case ID_UNDO:
                if (!m_Editor.Undo())
                    SetStatus("Nothing to undo");
                break;
            case ID_REDO:
                if (!m_Editor.Redo())
                    SetStatus("Nothing to redo");
                break;
            case ID_PLAY:
                PlayScene();
                break;
            case ID_MENU:
                ECS.GetResource<GameState>()->CurCameraState = StartMenu;
                GameSceneManager.SetActiveScene("TitleScreen");
                return false;
            default:
                break;
            }
        }
        x += b.Width + 8.0f;
    }

    std::string title = "Editing " + CurrentSceneName() + (m_Editor.IsDirty() ? " *" : "");
    Text(x + 10.0f, by + 7.0f, title, m_Editor.IsDirty() ? ACCENT : TEXT_DIM);
    return true;
}

void SceneEditorScene::RenderPalette()
{
    float top = SCREEN_H - TOOLBAR_H;
    float height = top - STATUS_H;
    DrawPanel(0, STATUS_H, PALETTE_W, height, PANEL_FILL, PANEL_BORDER);

    float x = 10.0f;
    float y = top - 28.0f;
    Text(x, y, "PALETTE", ACCENT);
    y -= 34.0f;

    std::string selectLabel = m_Tool == Tool::Select ? "> Select" : "Select";
    if (Button(ID_TOOL_SELECT, x, y, *m_UI, PALETTE_W - 20.0f, BUTTON_H, selectLabel))
        m_Tool = Tool::Select;
    y -= BUTTON_H + 6.0f;

    for (int i = 0; i < static_cast<int>(PrefabType::Count); ++i)
    {
        PrefabType type = static_cast<PrefabType>(i);
        bool active = m_Tool == Tool::Place && m_Prefab == type;
        std::string label = std::to_string(i + 1) + " " + Editor::PrefabName(type);
        if (active)
            label = "> " + label;
        if (Button(ID_PREFAB_FIRST + i, x, y, *m_UI, PALETTE_W - 20.0f, BUTTON_H, label))
            SelectPrefab(type);
        y -= BUTTON_H + 4.0f;
    }

    y -= 14.0f;
    Text(x, y, "BRUSH", ACCENT);
    y -= LINE_H + 6.0f;

    int delta = 0;
    int id = ID_BRUSH_STEPPERS;
    if (Stepper(id, x, y, PALETTE_W - 20.0f, "HP " + std::to_string(m_Brush.Health), delta))
        m_Brush.Health = std::max(1, m_Brush.Health + delta * 10);
    y -= LINE_H + 4.0f;
    id += 2;
    if (Stepper(id, x, y, PALETTE_W - 20.0f, "Bat " + std::to_string(m_Brush.Battalion), delta))
        m_Brush.Battalion = std::max(0, m_Brush.Battalion + delta);
    y -= LINE_H + 4.0f;
    id += 2;
    if (Stepper(id, x, y, PALETTE_W - 20.0f, "Cry " + std::to_string(m_Brush.CrystalAmount), delta))
        m_Brush.CrystalAmount = std::max(1, m_Brush.CrystalAmount + delta * 5);
    y -= LINE_H + 4.0f;
    id += 2;
    if (Stepper(id,
                x,
                y,
                PALETTE_W - 20.0f,
                "Spd " + Format("%.1f", m_Brush.EnemySpeed * 1000.0f),
                delta))
        m_Brush.EnemySpeed = std::max(0.0002f, m_Brush.EnemySpeed + delta * 0.0002f);
    y -= LINE_H + 4.0f;
    id += 2;
    if (Stepper(id, x, y, PALETTE_W - 20.0f, "Rot " + Format("%.0f", m_Brush.YawDegrees), delta))
        m_Brush.YawDegrees = std::fmod(m_Brush.YawDegrees + delta * ROTATE_STEP + 360.0f, 360.0f);
    y -= LINE_H + 12.0f;

    if (CheckBox(ID_SNAP, x, y, m_Snap, 16.0f, *m_UI, "Snap (G)"))
        m_Snap = !m_Snap;
}

void SceneEditorScene::RenderInspector()
{
    float left = SCREEN_W - INSPECTOR_W;
    float top = SCREEN_H - TOOLBAR_H;
    DrawPanel(left, STATUS_H, INSPECTOR_W, top - STATUS_H, PANEL_FILL, PANEL_BORDER);

    float x = left + 10.0f;
    float y = top - 28.0f;
    Text(x, y, "INSPECTOR", ACCENT);
    y -= 30.0f;

    Entity selected = m_Editor.Selected();
    int delta = 0;
    int id = ID_INSPECTOR_STEPPERS;
    if (selected == NULL_ENTITY)
    {
        Text(x, y, "Nothing selected", TEXT_DIM);
        y -= LINE_H;
        Text(x, y, "Click an object to edit", TEXT_DIM);
        y -= LINE_H * 2;
    }
    else
    {
        EntityKind kind = m_Editor.KindOf(selected);
        Text(x, y, std::string(Editor::KindName(kind)) + "  #" + std::to_string(selected));
        y -= LINE_H;
        Vec3 p = m_Editor.GetPosition(selected);
        Text(x, y, "Pos " + Format("%.1f", p.X) + ", " + Format("%.1f", p.Z), TEXT_DIM);
        y -= LINE_H;
        Text(x, y, "Rot " + Format("%.0f", m_Editor.GetYaw(selected)), TEXT_DIM);
        y -= LINE_H + 6.0f;

        if (m_Editor.GetHealth(selected) >= 0)
        {
            int step = kind == EntityKind::PlayerBase ? 100 : 10;
            if (Stepper(id,
                        x,
                        y,
                        INSPECTOR_W - 20.0f,
                        "Health " + std::to_string(m_Editor.GetHealth(selected)),
                        delta))
                m_Editor.SetHealth(selected, m_Editor.GetHealth(selected) + delta * step);
            y -= LINE_H + 4.0f;
        }
        id += 2;
        if (m_Editor.GetBattalion(selected) >= 0)
        {
            if (Stepper(id,
                        x,
                        y,
                        INSPECTOR_W - 20.0f,
                        "Battalion " + std::to_string(m_Editor.GetBattalion(selected)),
                        delta))
                m_Editor.SetBattalion(selected,
                                      std::max(0, m_Editor.GetBattalion(selected) + delta));
            y -= LINE_H + 4.0f;
        }
        id += 2;
        if (m_Editor.GetCrystalAmount(selected) >= 0)
        {
            if (Stepper(id,
                        x,
                        y,
                        INSPECTOR_W - 20.0f,
                        "Crystals " + std::to_string(m_Editor.GetCrystalAmount(selected)),
                        delta))
                m_Editor.SetCrystalAmount(selected,
                                          m_Editor.GetCrystalAmount(selected) + delta * 5);
            y -= LINE_H + 4.0f;
        }
        id += 2;
        if (m_Editor.GetEnemySpeed(selected) > 0.0f)
        {
            float speed = m_Editor.GetEnemySpeed(selected);
            if (Stepper(id,
                        x,
                        y,
                        INSPECTOR_W - 20.0f,
                        "Speed " + Format("%.1f", speed * 1000.0f),
                        delta))
                m_Editor.SetEnemySpeed(selected, std::max(0.0002f, speed + delta * 0.0002f));
            y -= LINE_H + 4.0f;
        }

        float half = (INSPECTOR_W - 30.0f) * 0.5f;
        if (Button(ID_ROTATE, x, y, *m_UI, half, BUTTON_H, "Rotate R"))
            RotateSelected(ROTATE_STEP);
        if (Button(ID_DELETE, x + half + 10.0f, y, *m_UI, half, BUTTON_H, "Delete X"))
            DeleteSelected();
        y -= BUTTON_H + 20.0f;
    }

    // -- Scene settings (GameState) ------------------------------------------------
    Text(x, y, "SCENE", ACCENT);
    y -= LINE_H + 6.0f;
    id = ID_SCENE_STEPPERS;
    if (Stepper(id,
                x,
                y,
                INSPECTOR_W - 20.0f,
                "Crystals " + std::to_string(m_Editor.GetStartingCrystals()),
                delta))
        m_Editor.SetStartingCrystals(m_Editor.GetStartingCrystals() + delta * 5);
    y -= LINE_H + 4.0f;
    id += 2;
    if (Stepper(id,
                x,
                y,
                INSPECTOR_W - 20.0f,
                "Round " + std::to_string(m_Editor.GetRoundNumber()),
                delta))
        m_Editor.SetRoundNumber(m_Editor.GetRoundNumber() + delta);
    y -= LINE_H + 4.0f;
    id += 2;
    if (Stepper(id,
                x,
                y,
                INSPECTOR_W - 20.0f,
                "Spawn vol " + std::to_string(m_Editor.GetSpawnVolume()),
                delta))
        m_Editor.SetSpawnVolume(m_Editor.GetSpawnVolume() + delta);
    y -= LINE_H + 10.0f;

    Text(x, y, "Objects " + std::to_string(m_Editor.EditableEntities().size()), TEXT_DIM);
    y -= LINE_H;
    Text(x,
         y,
         "Undo " + std::to_string(m_Editor.UndoCount()) + "  Redo " +
                 std::to_string(m_Editor.RedoCount()),
         TEXT_DIM);
    y -= LINE_H;

    std::vector<std::string> issues = m_Editor.Validate();
    if (issues.empty())
        Text(x, y, "Playable", TEXT_DIM);
    else
        Text(x, y, std::to_string(issues.size()) + " problem(s)", ERROR_TEXT);
}

void SceneEditorScene::RenderStatusBar()
{
    DrawPanel(0, 0, SCREEN_W, STATUS_H, PANEL_FILL, PANEL_BORDER);
    if (m_StatusTimer > 0.0f)
        Text(10.0f, 28.0f, m_Status, m_StatusIsError ? ERROR_TEXT : TEXT);
    Text(10.0f,
         8.0f,
         "WASD pan  Z/C zoom  1-7 prefab  Space select  R rotate  X delete  U/Y undo/redo",
         TEXT_DIM);
}

void SceneEditorScene::DrawWorldMarker(const Vec3& position, float size, float r, float g, float b)
{
    Vec2 s = m_Cam->WorldPointToScreenSpace(position);
    App::DrawLine(s.X - size, s.Y - size, s.X + size, s.Y - size, r, g, b);
    App::DrawLine(s.X + size, s.Y - size, s.X + size, s.Y + size, r, g, b);
    App::DrawLine(s.X + size, s.Y + size, s.X - size, s.Y + size, r, g, b);
    App::DrawLine(s.X - size, s.Y + size, s.X - size, s.Y - size, r, g, b);
}

void SceneEditorScene::RenderOverlay()
{
    // Play area border
    float half = 25.0f;
    Vec3 corners[4] = {{-half, 0, -half}, {half, 0, -half}, {half, 0, half}, {-half, 0, half}};
    for (int i = 0; i < 4; ++i)
    {
        Vec2 a = m_Cam->WorldPointToScreenSpace(corners[i]);
        Vec2 b = m_Cam->WorldPointToScreenSpace(corners[(i + 1) % 4]);
        App::DrawLine(a.X, a.Y, b.X, b.Y, 0.3f, 0.8f, 1.0f);
    }

    // Selection
    Entity selected = m_Editor.Selected();
    if (selected != NULL_ENTITY)
    {
        DrawWorldMarker(m_Editor.GetPosition(selected), 14.0f, ACCENT.R, ACCENT.G, ACCENT.B);
        DrawWorldMarker(m_Editor.GetPosition(selected), 16.0f, ACCENT.R, ACCENT.G, ACCENT.B);
    }

    if (MouseOverUI() || m_Dragging)
        return;
    Vec3 ground;
    if (!MouseToGround(ground))
        return;

    if (m_Tool == Tool::Place)
    {
        // Ghost cursor at the snapped placement point
        Vec3 snapped = Editor::SceneEditor::ClampToPlayArea(SnapPoint(ground));
        DrawWorldMarker(snapped, 8.0f, 0.4f, 1.0f, 0.4f);
        Text(m_UI->mouseX + 14.0f, m_UI->mouseY + 10.0f, Editor::PrefabName(m_Prefab));
    }
    else
    {
        // Hover highlight
        Entity hovered = m_Editor.Pick(ground);
        if (hovered != NULL_ENTITY && hovered != selected)
            DrawWorldMarker(m_Editor.GetPosition(hovered), 12.0f, 0.8f, 0.8f, 0.8f);
    }
}
