#include "SceneEditorScene.h"

#include "AssetServer.h"
#include "Camera.h"
#include "ECSManager.h"
#include "GameManager.h"
#include "GameOptions.h"
#include "Input.h"
#include "Lighting.h"
#include "Log.h"
#include "Mesh.h"
#include "ModelImport.h"
#include "Scripting/ScriptRegistry.h"
#include "UIState.h"
#include "Widget.h"
#include "World/SceneComponents.h"
#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

extern ECSManager ECS;
extern GameManager GameSceneManager;

using Editor::ObjectKind;
using SceneObjects::BodyType;

namespace
{
    // -- Layout (virtual screen coordinates, y up) ---------------------------------
    constexpr float SCREEN_W = static_cast<float>(APP_VIRTUAL_WIDTH);
    constexpr float SCREEN_H = static_cast<float>(APP_VIRTUAL_HEIGHT);
    constexpr float TOOLBAR_H = 50.0f;
    constexpr float STATUS_H = 50.0f;
    constexpr float PALETTE_W = 180.0f;
    constexpr float INSPECTOR_W = 250.0f;
    constexpr float BUTTON_H = 26.0f;
    constexpr float ROW_H = 26.0f;
    constexpr float STEP_W = 24.0f;
    constexpr float SWATCH = 22.0f;

    const Color PANEL_FILL = {0.12f, 0.13f, 0.16f};
    const Color PANEL_BORDER = {0.45f, 0.47f, 0.52f};
    const Color TEXT = {0.92f, 0.92f, 0.92f};
    const Color TEXT_DIM = {0.62f, 0.64f, 0.68f};
    const Color ACCENT = {1.0f, 0.82f, 0.25f};
    const Color ERROR_TEXT = {1.0f, 0.4f, 0.35f};
    const Color PLAY_TEXT = {0.4f, 1.0f, 0.5f};

    // -- Camera ------------------------------------------------------------------------
    constexpr float PAN_SPEED = 18.0f;
    constexpr float ZOOM_SPEED = 25.0f;
    constexpr float MIN_DISTANCE = 6.0f;
    constexpr float MAX_DISTANCE = 90.0f;

    constexpr float SNAP_STEP = 0.5f;
    constexpr float ROTATE_STEP = 15.0f;
    constexpr float SIZE_STEP = 0.25f;
    constexpr float STATUS_TIME = 4.0f;
    constexpr float DRAG_THRESHOLD = 0.01f;

    // Widgets that keep state across frames have fixed ids: the scene list
    // (open / closed) and the text fields (being edited). Buttons use
    // dynamic ids from FIRST_DYNAMIC_ID
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
    constexpr int ID_FIELD_PARAM = 20;
    constexpr int ID_FIELD_LAST_OBJECT = 39;
    // Scene tab fields
    constexpr int ID_FIELD_FIELD_W = 40;
    constexpr int ID_FIELD_FIELD_H = 41;
    constexpr int ID_FIELD_SCENE_PARAM = 42;
    constexpr int MAX_PARAM_FIELDS = 8;
    constexpr int FIRST_DYNAMIC_ID = 100;
    // Width of the labels in front of typed values
    constexpr float LABEL_W = 62.0f;
    const char* const TAGS[] = {"", "Player", "Enemy", "Pickup", "Wall", "Goal", "Hazard", "Spawner"};
    constexpr int TAG_COUNT = sizeof(TAGS) / sizeof(TAGS[0]);

    const Vec3 COLORS[] = {{0.90f, 0.30f, 0.25f},
                           {0.95f, 0.60f, 0.20f},
                           {0.95f, 0.85f, 0.30f},
                           {0.35f, 0.75f, 0.35f},
                           {0.25f, 0.70f, 0.70f},
                           {0.30f, 0.50f, 0.90f},
                           {0.60f, 0.40f, 0.85f},
                           {0.85f, 0.85f, 0.85f}};
    constexpr int COLOR_COUNT = sizeof(COLORS) / sizeof(COLORS[0]);

    std::string Fmt(const char* format, float value)
    {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), format, value);
        return buffer;
    }

    void Text(float x, float y, const std::string& text, const Color& c = TEXT)
    {
        App::Print(x, y, text.c_str(), c.R, c.G, c.B);
    }

    Color ToColor(const Vec3& v) { return Color(v.X, v.Y, v.Z); }

    bool SameColor(const Vec3& a, const Vec3& b)
    {
        return std::fabs(a.X - b.X) < 0.01f && std::fabs(a.Y - b.Y) < 0.01f &&
               std::fabs(a.Z - b.Z) < 0.01f;
    }

    // Cycle an index through [0, count) by delta
    int Cycle(int index, int delta, int count)
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

    std::string TagLabel(const std::string& tag) { return tag.empty() ? "-" : tag; }

    // [""] + names, used by the script pickers
    std::vector<std::string> WithNone(std::vector<std::string> names)
    {
        names.insert(names.begin(), "");
        return names;
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
    if (m_SceneDirectory.empty())
        m_SceneDirectory = GameManager::SCENES_DIRECTORY;
    m_Editor.OnWorldReplaced = [this] {
        // The world was replaced without per entity delete events
        GameSceneManager.ResetRenderCaches();
        m_Dragging = false;
    };
}

void SceneEditorScene::Setup()
{
    m_Light->SetLightPerspective(120.0f, m_Options->ScreenRatio, 0.1f, 1000.0f);
    m_Light->SetPositionAndTarget(Vec3(0.0f, 25.0f, -5.0f), Vec3(0.0f, 0.0f, 0.0f));
    m_Cam->SetProjectionPerspective();

    m_CamTarget = Vec3(0, 0, 0);
    m_CamDistance = 30.0f;
    m_Tool = Tool::Select;
    m_Dragging = false;
    m_PaletteDrag = false;
    m_PendingSceneIndex = -1;
    m_UI->openDropDownId = 0;
    m_UI->focusedItem = 0;
    m_Models = AssetServer::AvailableModels();
    if (m_Brush.Model.empty() && !m_Models.empty())
        m_Brush.Model = m_Models.front();
    std::error_code importDirError;
    std::filesystem::create_directories(IMPORT_DIRECTORY, importDirError);

    // Open the first scene of the folder, or start a new one
    RefreshSceneList();
    if (m_SceneNames.empty() || !OpenScene(m_SceneNames.front()))
        NewDocument();
    SetStatus("Pick a shape on the left and click (or drag it onto) the field. Tutorial: docs/EditorTutorial.md");
}

void SceneEditorScene::Update(float deltaTime)
{
    float deltaSeconds = deltaTime / 1000.0f;
    if (m_StatusTimer > 0.0f)
        m_StatusTimer -= deltaSeconds;

    // While a text field is being edited the keyboard types into it
    if (m_UI->IsTyping())
    {
        UpdateViewportMouse();
        return;
    }
    // While playing, the keyboard belongs to the scripts (only P stops)
    if (Input::WasPressed(App::KEY_P))
        TogglePlay();
    if (m_Editor.IsPlaying())
        return;

    UpdateCamera(deltaSeconds);
    UpdateShortcuts();
    UpdateViewportMouse();
}

void SceneEditorScene::Render()
{
    m_NextId = FIRST_DYNAMIC_ID;
    // While the scene list is open it covers other widgets: they must not
    // receive the click meant for a list entry
    bool listOpen = m_UI->openDropDownId == ID_SCENE_LIST;
    bool click = m_UI->leftClick;
    if (listOpen)
        m_UI->leftClick = false;

    // The inspector's text fields always edit the selected object: an edit
    // in progress is dropped when the selection changes
    if (m_Editor.Selected() != m_FieldsEntity)
    {
        if (m_UI->focusedItem >= ID_FIELD_FIRST_OBJECT && m_UI->focusedItem <= ID_FIELD_LAST_OBJECT)
            m_UI->focusedItem = 0;
        m_FieldsEntity = m_Editor.Selected();
    }

    RenderOverlay();
    RenderPalette();
    RenderInspector();
    RenderStatusBar();
    if (!RenderToolbar())
        return;

    m_UI->leftClick = click;
    if (!m_Editor.IsPlaying())
        RenderSceneList();
    if (listOpen && click && m_UI->openDropDownId == ID_SCENE_LIST &&
        m_UI->activeItem != ID_SCENE_LIST)
        m_UI->openDropDownId = 0;
}

//-----------------------------------------------------------------------------
// Input
//-----------------------------------------------------------------------------

void SceneEditorScene::UpdateCamera(float deltaSeconds)
{
    Vec3 pan(0, 0, 0);
    if (Input::IsDown(App::KEY_W))
        pan.Z += 1.0f;
    if (Input::IsDown(App::KEY_S))
        pan.Z -= 1.0f;
    if (Input::IsDown(App::KEY_A))
        pan.X += 1.0f;
    if (Input::IsDown(App::KEY_D))
        pan.X -= 1.0f;
    float speed = PAN_SPEED * (m_CamDistance / 30.0f) * deltaSeconds;
    m_CamTarget = m_Editor.ClampToField(m_CamTarget + pan * speed);

    if (Input::IsDown(App::KEY_Z))
        m_CamDistance -= ZOOM_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_C))
        m_CamDistance += ZOOM_SPEED * deltaSeconds;
    m_CamDistance = std::clamp(m_CamDistance, MIN_DISTANCE, MAX_DISTANCE);

    SceneObjects::ApplyCamera(*m_Cam, m_CamTarget, m_CamDistance);
}

void SceneEditorScene::UpdateShortcuts()
{
    const App::Key kindKeys[] = {App::KEY_1, App::KEY_2, App::KEY_3, App::KEY_4, App::KEY_5};
    for (int i = 0; i < static_cast<int>(ObjectKind::Count); ++i)
    {
        if (Input::WasPressed(kindKeys[i]))
            SelectKind(static_cast<ObjectKind>(i));
    }
    if (Input::WasPressed(App::KEY_SPACE))
        m_Tool = Tool::Select;
    if (Input::WasPressed(App::KEY_R))
        RotateSelected(ROTATE_STEP);
    if (Input::WasPressed(App::KEY_X))
        DeleteSelected();
    if (Input::WasPressed(App::KEY_F))
        DuplicateSelected();
    if (Input::WasPressed(App::KEY_U) && m_Editor.Undo())
        SetStatus("Undo");
    if (Input::WasPressed(App::KEY_Y) && m_Editor.Redo())
        SetStatus("Redo");
    if (Input::WasPressed(App::KEY_G))
        m_Snap = !m_Snap;
}

bool SceneEditorScene::MouseOverUI() const
{
    float x = m_UI->mouseX;
    float y = m_UI->mouseY;
    return m_UI->openDropDownId != 0 || x < PALETTE_W || x > SCREEN_W - INSPECTOR_W ||
           y > SCREEN_H - TOOLBAR_H || y < STATUS_H;
}

bool SceneEditorScene::MouseAtHeight(float height, Vec3& point) const
{
    Vec3 planePoint(0, height, 0);
    Vec3 planeNormal(0, 1, 0);
    point = m_Cam->ScreenSpaceToWorldPoint(m_UI->mouseX, m_UI->mouseY, planePoint, planeNormal);
    return point.IsValid();
}

Entity SceneEditorScene::PickUnderMouse(float* hitHeight) const
{
    return m_Editor.PickRay(
            [this](float height) {
                Vec3 point;
                MouseAtHeight(height, point);
                return point;
            },
            hitHeight);
}

Vec3 SceneEditorScene::SnapPoint(const Vec3& point) const
{
    if (!m_Snap)
        return point;
    return {Editor::SceneEditor::Snap(point.X, SNAP_STEP),
            point.Y,
            Editor::SceneEditor::Snap(point.Z, SNAP_STEP)};
}

void SceneEditorScene::BeginDrag(Entity entity, float height)
{
    // Keep the grabbed point under the mouse: drag on the plane at the
    // height it was grabbed
    Vec3 grabbed;
    if (!MouseAtHeight(height, grabbed))
        return;
    m_Dragging = true;
    m_DragMoved = false;
    m_DragHeight = height;
    m_DragOffset = SceneObjects::GetPosition(entity) - grabbed;
    m_DragOffset.Y = 0.0f;
}

void SceneEditorScene::UpdateViewportMouse()
{
    // A palette shape dragged onto the field is dropped where it is released
    if (m_PaletteDrag && !m_UI->mouseLeftDown)
    {
        m_PaletteDrag = false;
        Vec3 ground;
        if (!MouseOverUI() && MouseToGround(ground))
        {
            Entity placed = m_Editor.Place(m_Kind, SnapPoint(ground), m_Brush);
            if (placed != NULL_ENTITY)
                SetStatus("Placed " + m_Editor.NameOf(placed));
        }
        return;
    }

    if (m_Dragging && !m_UI->mouseLeftDown)
    {
        m_Dragging = false;
        if (m_DragMoved)
            SetStatus("Moved " + m_Editor.NameOf(m_Editor.Selected()));
    }

    if (m_Dragging)
    {
        Vec3 point;
        if (!MouseAtHeight(m_DragHeight, point))
            return;
        Entity selected = m_Editor.Selected();
        Vec3 target = SnapPoint(point + m_DragOffset);
        Vec3 delta = target - SceneObjects::GetPosition(selected);
        if (std::fabs(delta.X) > DRAG_THRESHOLD || std::fabs(delta.Z) > DRAG_THRESHOLD)
        {
            // One undo step for the whole drag, recorded once it really moves
            if (!m_DragRecorded)
            {
                m_Editor.RecordUndo();
                m_DragRecorded = true;
            }
            m_Editor.Move(selected, target, false);
            m_DragMoved = true;
        }
        return;
    }

    if (MouseOverUI() || m_PaletteDrag)
        return;
    // The first click outside a text field only finishes the edit
    if (m_UI->IsTyping())
        return;

    if (m_UI->rightClick)
    {
        m_Tool = Tool::Select;
        m_Editor.Select(NULL_ENTITY);
        return;
    }
    if (!m_UI->leftClick)
        return;

    // Pressing an object grabs it (also while placing), so objects can
    // always be dragged
    float grabHeight = 0.0f;
    Entity picked = PickUnderMouse(&grabHeight);
    if (picked != NULL_ENTITY && !m_Editor.IsField(picked))
    {
        m_Editor.Select(picked);
        m_DragRecorded = false;
        BeginDrag(picked, grabHeight);
        return;
    }

    Vec3 ground;
    if (!MouseToGround(ground))
        return;
    if (m_Tool == Tool::Place)
    {
        if (m_Kind == ObjectKind::Model && m_Brush.Model.empty())
        {
            SetStatus("No models found in data/models", true);
            return;
        }
        Entity placed = m_Editor.Place(m_Kind, SnapPoint(ground), m_Brush);
        SetStatus("Placed " + m_Editor.NameOf(placed) + " (keep the button down to drag it)");
        // Place and drag in one gesture; placing already recorded the undo step
        m_DragRecorded = true;
        BeginDrag(placed, 0.0f);
        return;
    }
    // Clicking the field selects it (to resize / recolour it)
    m_Editor.Select(picked);
}

//-----------------------------------------------------------------------------
// Actions
//-----------------------------------------------------------------------------

void SceneEditorScene::SelectKind(ObjectKind kind)
{
    m_Kind = kind;
    m_Tool = Tool::Place;
    SetStatus(std::string("Placing ") + Editor::ObjectKindName(kind) +
              ". Click the field (or drag the button onto it), right click to stop");
}

void SceneEditorScene::DeleteSelected()
{
    Entity selected = m_Editor.Selected();
    if (selected == NULL_ENTITY)
        return;
    std::string name = m_Editor.NameOf(selected);
    if (m_Editor.Remove(selected))
        SetStatus("Deleted " + name);
    else
        SetStatus("The field can not be deleted", true);
}

void SceneEditorScene::DuplicateSelected()
{
    Entity copy = m_Editor.Duplicate(m_Editor.Selected());
    if (copy != NULL_ENTITY)
        SetStatus("Created " + m_Editor.NameOf(copy));
}

void SceneEditorScene::RotateSelected(float degrees)
{
    Entity selected = m_Editor.Selected();
    if (selected != NULL_ENTITY)
        m_Editor.SetYaw(selected, SceneObjects::GetYaw(selected) + degrees);
}

void SceneEditorScene::TogglePlay()
{
    if (m_Editor.IsPlaying())
    {
        // Stop: drop the running scripts, then restore the authored world
        GameSceneManager.Scripts().Reset();
        m_Editor.EndPlay();
        SetStatus("Stopped, the scene is back to how it was before Play");
        return;
    }
    std::vector<std::string> issues = m_Editor.Validate();
    if (!issues.empty())
    {
        SetStatus("Can not play: " + issues.front(), true);
        return;
    }
    m_Dragging = false;
    m_PaletteDrag = false;
    m_UI->openDropDownId = 0;
    m_UI->focusedItem = 0;
    m_Editor.BeginPlay();
    GameSceneManager.Scripts().Reset();
    SetStatus("Playing. Press P or Stop to go back to editing");
}

void SceneEditorScene::SetStatus(const std::string& message, bool error)
{
    // Everything the status bar says also goes to the debug log
    if (error)
        LOG_WARN("Editor", "%s", message.c_str());
    else
        LOG_INFO("Editor", "%s", message.c_str());
    m_Status = message;
    m_StatusIsError = error;
    m_StatusTimer = STATUS_TIME;
}

//-----------------------------------------------------------------------------
// Scene documents
//-----------------------------------------------------------------------------

std::string SceneEditorScene::ScenePath(const std::string& name) const
{
    return m_SceneDirectory + "/" + name + GameManager::SCENE_EXTENSION;
}

void SceneEditorScene::RefreshSceneList()
{
    m_SceneNames.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_SceneDirectory, ec))
    {
        if (entry.is_regular_file() && entry.path().extension() == GameManager::SCENE_EXTENSION)
            m_SceneNames.push_back(entry.path().stem().string());
    }
    // The open scene is listed even before its first save
    if (!m_DocName.empty() && std::find(m_SceneNames.begin(), m_SceneNames.end(), m_DocName) == m_SceneNames.end())
        m_SceneNames.push_back(m_DocName);
    std::sort(m_SceneNames.begin(), m_SceneNames.end());
    m_SceneIndex = m_DocName.empty() ? 0 : IndexOf(m_SceneNames, m_DocName);
}

std::string SceneEditorScene::UniqueSceneName() const
{
    for (int i = 1;; ++i)
    {
        std::string name = "scene_" + std::to_string(i);
        std::error_code ec;
        if (std::find(m_SceneNames.begin(), m_SceneNames.end(), name) == m_SceneNames.end() &&
            !std::filesystem::exists(ScenePath(name), ec))
            return name;
    }
}

void SceneEditorScene::NewDocument()
{
    std::error_code ec;
    std::filesystem::create_directories(m_SceneDirectory, ec);
    std::string name = UniqueSceneName();
    m_Editor.NewScene();
    m_DocName = name;
    // Saved right away: the new scene is a real entry of the scene list
    Serialization::SaveResult result = m_Editor.SaveScene(ScenePath(name), name);
    RefreshSceneList();
    m_PendingSceneIndex = -1;
    if (result)
        SetStatus("Created " + name + ". Click its name (top right) to rename it");
    else
        SetStatus("Created " + name + " (not saved: " + result.Error + ")", true);
}

bool SceneEditorScene::OpenScene(const std::string& name)
{
    Serialization::LoadResult result = m_Editor.LoadScene(ScenePath(name));
    if (!result)
    {
        SetStatus("Could not open " + name + ": " + result.Error, true);
        m_SceneIndex = IndexOf(m_SceneNames, m_DocName);
        return false;
    }
    m_DocName = name;
    m_PendingSceneIndex = -1;
    RefreshSceneList();
    std::vector<std::uint8_t> head;
    std::string readError;
    bool isText = Serialization::WorldSerializer::ReadFile(ScenePath(name), head, readError) &&
                  Serialization::WorldSerializer::IsTextSave(head);
    if (!result.Warnings.empty())
        SetStatus("Opened " + name + " with problems: " + result.Warnings.front(), true);
    else
        SetStatus("Opened " + name + (isText ? " (plain text file)" : ""));
    return true;
}

bool SceneEditorScene::SaveDocument()
{
    Serialization::SaveResult result = m_Editor.SaveScene(ScenePath(m_DocName), m_DocName);
    if (!result)
    {
        SetStatus("Save failed: " + result.Error, true);
        return false;
    }
    SetStatus("Saved " + ScenePath(m_DocName));
    RefreshSceneList();
    return true;
}

void SceneEditorScene::RevertScene()
{
    std::error_code ec;
    if (!std::filesystem::exists(ScenePath(m_DocName), ec))
    {
        SetStatus(m_DocName + " was never saved", true);
        return;
    }
    OpenScene(m_DocName);
}

bool SceneEditorScene::RenameDocument(const std::string& newName)
{
    if (newName.empty() || newName == m_DocName)
        return false;
    std::error_code ec;
    if (std::filesystem::exists(ScenePath(newName), ec))
    {
        SetStatus("A scene called " + newName + " already exists", true);
        return false;
    }
    std::string oldPath = ScenePath(m_DocName);
    if (std::filesystem::exists(oldPath, ec))
    {
        std::filesystem::rename(oldPath, ScenePath(newName), ec);
        if (ec)
        {
            SetStatus("Could not rename: " + ec.message(), true);
            return false;
        }
    }
    std::string oldName = m_DocName;
    m_DocName = newName;
    ECS.GetResource<SceneSettings>()->Name = newName;
    RefreshSceneList();
    SetStatus("Renamed " + oldName + " to " + newName);
    return true;
}

//-----------------------------------------------------------------------------
// Drawing
//-----------------------------------------------------------------------------

int SceneEditorScene::Stepper(
        float x, float y, float width, const std::string& label, const char* minus, const char* plus)
{
    // "label        [-] [+]" row, returns -1 / +1 when a button was clicked
    Text(x, y + 7.0f, label);
    float right = x + width;
    int delta = 0;
    if (Button(NextId(), right - 2 * STEP_W - 4.0f, y, *m_UI, STEP_W, 22.0f, minus))
        delta = -1;
    if (Button(NextId(), right - STEP_W, y, *m_UI, STEP_W, 22.0f, plus))
        delta = 1;
    return delta;
}

bool SceneEditorScene::NumberRow(
        float x, float y, float width, const std::string& label, int fieldId, float& value, float step, const char* format)
{
    // "label [ value ] [-] [+]": type a value (Enter) or step it
    Text(x, y + 7.0f, label);
    float fieldX = x + LABEL_W;
    float fieldW = width - LABEL_W - 2 * STEP_W - 10.0f;
    bool changed = false;
    std::string text = Fmt(format, value);
    if (TextField(fieldId, fieldX, y, fieldW, 22.0f, *m_UI, text, TextFilter::Number) == TextFieldEvent::Committed)
    {
        char* end = nullptr;
        float typed = std::strtof(text.c_str(), &end);
        if (end != text.c_str() && std::isfinite(typed))
        {
            value = typed;
            changed = true;
        }
        else if (!text.empty())
            SetStatus("Not a number: " + text, true);
    }
    float right = x + width;
    if (Button(NextId(), right - 2 * STEP_W - 4.0f, y, *m_UI, STEP_W, 22.0f, "-"))
    {
        value -= step;
        changed = true;
    }
    if (Button(NextId(), right - STEP_W, y, *m_UI, STEP_W, 22.0f, "+"))
    {
        value += step;
        changed = true;
    }
    return changed;
}

bool SceneEditorScene::RenderToolbar()
{
    float y = SCREEN_H - TOOLBAR_H;
    DrawPanel(0, y, SCREEN_W, TOOLBAR_H, PANEL_FILL, PANEL_BORDER);
    float by = y + (TOOLBAR_H - BUTTON_H) * 0.5f;
    float x = 200.0f;
    bool playing = m_Editor.IsPlaying();

    auto toolbarButton = [&](const char* label, float width) {
        bool clicked = Button(NextId(), x, by, *m_UI, width, BUTTON_H, label);
        x += width + 8.0f;
        return clicked;
    };

    if (!playing)
    {
        if (toolbarButton("New", 50))
            NewDocument();
        if (toolbarButton("Revert", 60))
            RevertScene();
        if (toolbarButton("Save", 50))
            SaveDocument();
        if (toolbarButton("Undo", 50) && !m_Editor.Undo())
            SetStatus("Nothing to undo");
        if (toolbarButton("Redo", 50) && !m_Editor.Redo())
            SetStatus("Nothing to redo");
    }
    else
    {
        x += 4 * 58.0f + 68.0f;
    }
    if (toolbarButton(playing ? "Stop" : "Play", 60))
        TogglePlay();
    if (!playing && toolbarButton(m_ShowScene ? "Object" : "Scene", 70))
        m_ShowScene = !m_ShowScene;

    if (playing)
    {
        Text(x + 90.0f, by + 7.0f, "PLAYING " + m_DocName, PLAY_TEXT);
        return true;
    }
    // The scene's name: click it to rename the scene (and its file)
    Text(x + 4.0f, by + 7.0f, "Name", TEXT_DIM);
    std::string name = m_DocName;
    if (TextField(ID_FIELD_SCENE_NAME, x + 50.0f, by + 2.0f, 150.0f, 22.0f, *m_UI, name, TextFilter::Name) ==
        TextFieldEvent::Committed)
        RenameDocument(name);
    if (m_Editor.IsDirty())
        Text(x + 206.0f, by + 7.0f, "*", ACCENT);
    return true;
}

void SceneEditorScene::RenderSceneList()
{
    int before = m_SceneIndex;
    if (!DropdownList(ID_SCENE_LIST,
                      10.0f,
                      SCREEN_H - TOOLBAR_H + (TOOLBAR_H - BUTTON_H) * 0.5f,
                      180.0f,
                      BUTTON_H,
                      *m_UI,
                      m_SceneNames,
                      m_SceneIndex))
        return;
    if (m_SceneIndex < 0 || m_SceneIndex >= static_cast<int>(m_SceneNames.size()))
        return;
    const std::string picked = m_SceneNames[m_SceneIndex];
    if (picked == m_DocName)
    {
        m_PendingSceneIndex = -1;
        return;
    }
    // Unsaved changes are only thrown away when the scene is picked twice
    if (m_Editor.IsDirty() && m_PendingSceneIndex != m_SceneIndex)
    {
        m_PendingSceneIndex = m_SceneIndex;
        m_SceneIndex = before;
        SetStatus(m_DocName + " has unsaved changes: Save, or pick " + picked + " again to discard them", true);
        return;
    }
    OpenScene(picked);
}

void SceneEditorScene::RenderPalette()
{
    if (m_Editor.IsPlaying())
        return;
    float top = SCREEN_H - TOOLBAR_H;
    DrawPanel(0, STATUS_H, PALETTE_W, top - STATUS_H, PANEL_FILL, PANEL_BORDER);
    float x = 10.0f;
    float width = PALETTE_W - 20.0f;
    float y = top - 26.0f;
    Text(x, y, "PALETTE", ACCENT);
    y -= 32.0f;

    if (Button(NextId(), x, y, *m_UI, width, BUTTON_H, m_Tool == Tool::Select ? "> Select" : "Select"))
        m_Tool = Tool::Select;
    y -= BUTTON_H + 4.0f;
    for (int i = 0; i < static_cast<int>(ObjectKind::Count); ++i)
    {
        ObjectKind kind = static_cast<ObjectKind>(i);
        std::string label = std::to_string(i + 1) + " " + Editor::ObjectKindName(kind);
        if (m_Tool == Tool::Place && m_Kind == kind)
            label = "> " + label;
        if (Button(NextId(), x, y, *m_UI, width, BUTTON_H, label))
        {
            SelectKind(kind);
            // Keep the button down and drag it onto the field to drop it there
            m_PaletteDrag = true;
        }
        y -= BUTTON_H + 4.0f;
    }

    y -= 8.0f;
    Text(x, y, "BRUSH", ACCENT);
    y -= ROW_H + 2.0f;
    int d = 0;
    if ((d = Stepper(x, y, width, "W " + Fmt("%.2f", m_Brush.Width))) != 0)
        m_Brush.Width = std::max(0.25f, m_Brush.Width + d * SIZE_STEP);
    y -= ROW_H;
    if ((d = Stepper(x, y, width, "H " + Fmt("%.2f", m_Brush.Height))) != 0)
        m_Brush.Height = std::max(0.25f, m_Brush.Height + d * SIZE_STEP);
    y -= ROW_H;
    if ((d = Stepper(x, y, width, "Sides " + std::to_string(m_Brush.Sides))) != 0)
        m_Brush.Sides = std::clamp(m_Brush.Sides + d, 3, 12);
    y -= ROW_H;
    if ((d = Stepper(x, y, width, "Rot " + Fmt("%.0f", m_Brush.YawDegrees))) != 0)
        m_Brush.YawDegrees = std::fmod(m_Brush.YawDegrees + d * ROTATE_STEP + 360.0f, 360.0f);
    y -= ROW_H;
    if ((d = Stepper(x, y, width, SceneObjects::BodyTypeName(m_Brush.Body), "<", ">")) != 0)
        m_Brush.Body = static_cast<BodyType>(
                Cycle(static_cast<int>(m_Brush.Body), d, static_cast<int>(BodyType::Count)));
    y -= ROW_H;
    if ((d = Stepper(x, y, width, "Tag " + TagLabel(m_Brush.Tag), "<", ">")) != 0)
        m_Brush.Tag = TAGS[Cycle(IndexOf(TAGS, m_Brush.Tag), d, TAG_COUNT)];
    y -= ROW_H;
    if (!m_Models.empty())
    {
        if ((d = Stepper(x, y, width, m_Brush.Model.substr(0, 9), "<", ">")) != 0)
        {
            int count = static_cast<int>(m_Models.size());
            m_Brush.Model = m_Models[Cycle(IndexOf(m_Models, m_Brush.Model), d, count)];
        }
        y -= ROW_H;
    }

    y -= SWATCH;
    for (int i = 0; i < COLOR_COUNT; ++i)
    {
        float sx = x + (i % 4) * (SWATCH + 10.0f);
        float sy = y - (i / 4) * (SWATCH + 8.0f);
        if (ColorSwatch(NextId(), sx, sy, SWATCH, ToColor(COLORS[i]), SameColor(m_Brush.Color, COLORS[i]), *m_UI))
            m_Brush.Color = COLORS[i];
    }
    y -= 2 * (SWATCH + 8.0f);
    if (CheckBox(NextId(), x, y, m_Snap, 16.0f, *m_UI, "Snap (G)"))
        m_Snap = !m_Snap;

    // Import a custom .obj: type a path (or a file name in data/import/),
    // Enter or the button imports it into data/models
    y -= 34.0f;
    Text(x, y, "IMPORT .OBJ", ACCENT);
    y -= 28.0f;
    bool imported = false;
    if (TextField(ID_FIELD_IMPORT, x, y, width, 22.0f, *m_UI, m_ImportPath, TextFilter::Any, 260) ==
        TextFieldEvent::Committed)
    {
        ImportModel();
        imported = true;
    }
    y -= 28.0f;
    if (Button(NextId(), x, y, *m_UI, width, BUTTON_H, "Import model") && !imported)
        ImportModel();
}

void SceneEditorScene::ImportModel()
{
    std::string path = m_ImportPath;
    // A bare file name is looked up in the import folder
    std::error_code ec;
    if (!path.empty() && !std::filesystem::exists(path, ec) && path.find_first_of("/\\") == std::string::npos)
    {
        std::filesystem::path inFolder = std::filesystem::path(IMPORT_DIRECTORY) / path;
        if (inFolder.extension() != ".obj" && !std::filesystem::exists(inFolder, ec))
            inFolder += ".obj";
        if (std::filesystem::exists(inFolder, ec))
            path = inFolder.string();
    }
    if (path.empty())
    {
        SetStatus("Type the path of an .obj file (or its name in data/import/) first", true);
        return;
    }
    ModelImport::Result result = ModelImport::Import(path, AssetServer::MODEL_DIRECTORY);
    if (!result)
    {
        SetStatus("Import failed: " + result.Error, true);
        return;
    }
    m_Models = AssetServer::AvailableModels();
    m_Brush.Model = result.Name;
    SelectKind(ObjectKind::Model);
    std::string message = (result.AlreadyImported ? "Already imported: " : "Imported ") + result.Name + " (" +
                          std::to_string(result.Triangles) + " triangles). Click the field to place it";
    if (!result.Warnings.empty())
        message += ". " + result.Warnings.front();
    SetStatus(message, false);
    m_ImportPath.clear();
}

void SceneEditorScene::RenderInspector()
{
    float left = SCREEN_W - INSPECTOR_W;
    float top = SCREEN_H - TOOLBAR_H;
    DrawPanel(left, STATUS_H, INSPECTOR_W, top - STATUS_H, PANEL_FILL, PANEL_BORDER);
    float x = left + 10.0f;
    float y = top - 26.0f;
    if (m_Editor.IsPlaying())
    {
        Text(x, y, "PLAYING", PLAY_TEXT);
        y -= ROW_H;
        Text(x, y, "Scripts: " + std::to_string(GameSceneManager.Scripts().InstanceCount()), TEXT_DIM);
        y -= ROW_H;
        for (const std::string& missing : GameSceneManager.Scripts().MissingScripts())
        {
            Text(x, y, "Missing " + missing, ERROR_TEXT);
            y -= ROW_H;
        }
        return;
    }
    if (m_ShowScene)
        RenderSceneInspector(x, y);
    else
        RenderObjectInspector(x, y);
}

void SceneEditorScene::RenderObjectInspector(float x, float& y)
{
    float width = INSPECTOR_W - 20.0f;
    Text(x, y, "OBJECT", ACCENT);
    Text(x + 80.0f, y, "click a value to type it", TEXT_DIM);
    y -= 30.0f;
    Entity e = m_Editor.Selected();
    if (e == NULL_ENTITY)
    {
        Text(x, y, "Nothing selected", TEXT_DIM);
        y -= ROW_H;
        Text(x, y, "Click an object to edit it", TEXT_DIM);
        return;
    }

    bool isShape = ECS.HasComponent<Shape2D>(e);
    bool isField = m_Editor.IsField(e);
    std::string kind = Editor::ObjectKindName(m_Editor.KindOf(e));

    if (isField)
    {
        Text(x, y + 7.0f, "Field  #" + std::to_string(e), TEXT_DIM);
        y -= ROW_H;
    }
    else
    {
        // Name: type a new one (must be unique)
        Text(x, y + 7.0f, "Name");
        std::string name = m_Editor.NameOf(e);
        if (TextField(ID_FIELD_NAME, x + LABEL_W, y, width - LABEL_W, 22.0f, *m_UI, name) ==
                    TextFieldEvent::Committed &&
            name != m_Editor.NameOf(e) && !m_Editor.Rename(e, name))
            SetStatus("Can not rename to '" + name + "' (empty or already used)", true);
        y -= ROW_H;
        Text(x, y + 7.0f, kind + "  #" + std::to_string(e), TEXT_DIM);
        y -= ROW_H;

        // Position / rotation
        Vec3 p = SceneObjects::GetPosition(e);
        float px = p.X, pz = p.Z, yaw = SceneObjects::GetYaw(e);
        if (NumberRow(x, y, width, "Pos X", ID_FIELD_POS_X, px, SNAP_STEP))
            m_Editor.Move(e, Vec3(px, p.Y, p.Z));
        y -= ROW_H;
        if (NumberRow(x, y, width, "Pos Z", ID_FIELD_POS_Z, pz, SNAP_STEP))
            m_Editor.Move(e, Vec3(p.X, p.Y, pz));
        y -= ROW_H;
        if (NumberRow(x, y, width, "Rot", ID_FIELD_ROT, yaw, ROTATE_STEP, "%.0f"))
            m_Editor.SetYaw(e, yaw);
        y -= ROW_H + 4.0f;
    }

    if (isShape)
    {
        Shape2D shape = ECS.GetComponent<Shape2D>(e);
        bool round = shape.Type == Shape2DType::Circle || shape.Type == Shape2DType::Polygon;
        float w = shape.Width, h = shape.Height, thick = shape.Thickness, sides = static_cast<float>(shape.Sides);
        if (NumberRow(x, y, width, round ? "Size" : "Width", ID_FIELD_WIDTH, w, SIZE_STEP))
            m_Editor.SetSize(e, w, shape.Height);
        y -= ROW_H;
        if (!round)
        {
            if (NumberRow(x, y, width, "Height", ID_FIELD_HEIGHT, h, SIZE_STEP))
                m_Editor.SetSize(e, shape.Width, h);
            y -= ROW_H;
        }
        if (shape.Type == Shape2DType::Polygon)
        {
            if (NumberRow(x, y, width, "Sides", ID_FIELD_SIDES, sides, 1.0f, "%.0f"))
                m_Editor.SetSides(e, static_cast<int>(std::lround(sides)));
            y -= ROW_H;
        }
        if (!isField)
        {
            if (NumberRow(x, y, width, "Thick", ID_FIELD_THICK, thick, 0.05f))
                m_Editor.SetThickness(e, thick);
            y -= ROW_H;
        }
        y -= SWATCH - 4.0f;
        for (int i = 0; i < COLOR_COUNT; ++i)
        {
            float sx = x + i * (SWATCH + 6.0f);
            if (ColorSwatch(NextId(), sx, y, SWATCH, ToColor(COLORS[i]), SameColor(shape.Color, COLORS[i]), *m_UI))
                m_Editor.SetColor(e, COLORS[i]);
        }
        y -= ROW_H + 2.0f;
    }
    else
    {
        float scale = ECS.GetComponent<Transform>(e).LocalScale.X;
        if (NumberRow(x, y, width, "Scale", ID_FIELD_SCALE, scale, SIZE_STEP))
            m_Editor.SetSize(e, scale, scale);
        y -= ROW_H;
        if (!m_Models.empty())
        {
            const std::string& model = ECS.GetComponent<Mesh>(e).Model;
            int d = 0;
            if ((d = Stepper(x, y, width, "Model " + model.substr(0, 10), "<", ">")) != 0)
            {
                int count = static_cast<int>(m_Models.size());
                m_Editor.SetModel(e, m_Models[Cycle(IndexOf(m_Models, model), d, count)]);
            }
            y -= ROW_H;
        }
    }

    if (isField)
    {
        Text(x, y, "The field holds the scene,", TEXT_DIM);
        y -= ROW_H;
        Text(x, y, "size it in the Scene tab", TEXT_DIM);
        return;
    }

    int d = 0;
    BodyType body = SceneObjects::GetBodyType(e);
    if ((d = Stepper(x, y, width, std::string("Body ") + SceneObjects::BodyTypeName(body), "<", ">")) != 0)
        m_Editor.SetBody(e, static_cast<BodyType>(Cycle(static_cast<int>(body), d, static_cast<int>(BodyType::Count))));
    y -= ROW_H;
    std::string tag = ECS.GetComponent<SceneObject>(e).Tag;
    if ((d = Stepper(x, y, width, "Tag " + TagLabel(tag), "<", ">")) != 0)
        m_Editor.SetTag(e, TAGS[Cycle(IndexOf(TAGS, tag), d, TAG_COUNT)]);
    y -= ROW_H;

    // Script picker + its declared parameters (typed or stepped)
    std::vector<std::string> scripts = WithNone(ScriptRegistry::Get().Names(false));
    std::string script = m_Editor.GetScript(e);
    std::string scriptLabel = script.empty() ? "-" : script;
    if ((d = Stepper(x, y, width, "Script " + scriptLabel.substr(0, 11), "<", ">")) != 0)
    {
        int count = static_cast<int>(scripts.size());
        m_Editor.SetScript(e, scripts[Cycle(IndexOf(scripts, script), d, count)]);
    }
    y -= ROW_H;
    if (const ScriptInfo* info = ScriptRegistry::Get().Find(script))
    {
        int index = 0;
        for (const ScriptParam& param : info->Params)
        {
            if (index >= MAX_PARAM_FIELDS)
                break;
            float value = m_Editor.GetScriptParam(e, param.Name);
            if (NumberRow(x + 10.0f, y, width - 10.0f, param.Name.substr(0, 7), ID_FIELD_PARAM + index, value, param.Step))
                m_Editor.SetScriptParam(e, param.Name, value);
            y -= ROW_H;
            ++index;
        }
    }

    y -= 6.0f;
    float third = (width - 16.0f) / 3.0f;
    if (Button(NextId(), x, y, *m_UI, third, BUTTON_H, "Rot R"))
        RotateSelected(ROTATE_STEP);
    if (Button(NextId(), x + third + 8.0f, y, *m_UI, third, BUTTON_H, "Dup F"))
        DuplicateSelected();
    if (Button(NextId(), x + 2 * (third + 8.0f), y, *m_UI, third, BUTTON_H, "Del X"))
        DeleteSelected();
}

void SceneEditorScene::RenderSceneInspector(float x, float& y)
{
    float width = INSPECTOR_W - 20.0f;
    auto settings = ECS.GetResource<SceneSettings>();
    Text(x, y, "SCENE", ACCENT);
    y -= 30.0f;

    int d = 0;
    std::vector<std::string> scripts = WithNone(ScriptRegistry::Get().Names(true));
    std::string script = settings->SceneScript;
    std::string scriptLabel = script.empty() ? "-" : script;
    if ((d = Stepper(x, y, width, "Script " + scriptLabel.substr(0, 11), "<", ">")) != 0)
    {
        int count = static_cast<int>(scripts.size());
        m_Editor.SetSceneScript(scripts[Cycle(IndexOf(scripts, script), d, count)]);
    }
    y -= ROW_H;
    if (const ScriptInfo* info = ScriptRegistry::Get().Find(settings->SceneScript))
    {
        int index = 0;
        for (const ScriptParam& param : info->Params)
        {
            if (index >= MAX_PARAM_FIELDS)
                break;
            float value = m_Editor.GetSceneParam(param.Name);
            if (NumberRow(x + 10.0f, y, width - 10.0f, param.Name.substr(0, 7), ID_FIELD_SCENE_PARAM + index, value, param.Step))
                m_Editor.SetSceneParam(param.Name, value);
            y -= ROW_H;
            ++index;
        }
    }

    y -= 6.0f;
    float fieldW = settings->FieldWidth;
    float fieldH = settings->FieldHeight;
    if (NumberRow(x, y, width, "Field W", ID_FIELD_FIELD_W, fieldW, 2.0f, "%.0f"))
        m_Editor.SetFieldSize(fieldW, settings->FieldHeight);
    y -= ROW_H;
    if (NumberRow(x, y, width, "Field H", ID_FIELD_FIELD_H, fieldH, 2.0f, "%.0f"))
        m_Editor.SetFieldSize(settings->FieldWidth, fieldH);
    y -= ROW_H + 6.0f;

    if (Button(NextId(), x, y, *m_UI, width, BUTTON_H, "Game camera = view"))
    {
        m_Editor.SetGameCamera(m_CamTarget, m_CamDistance);
        SetStatus("The game will start with the current view");
    }
    y -= BUTTON_H + 10.0f;

    // Scene files in plain text (readable / diffable) or binary
    bool text = Serialization::WorldSerializer::FileFormat() == Serialization::SaveFormat::Text;
    if (CheckBox(NextId(), x, y, text, 16.0f, *m_UI, "Plain text files"))
    {
        text = !text;
        Serialization::WorldSerializer::SetFileFormat(text ? Serialization::SaveFormat::Text
                                                           : Serialization::SaveFormat::Binary);
        SetStatus(text ? "Scenes are saved as plain text (Save to write it)" : "Scenes are saved as binary");
    }
    y -= ROW_H + 8.0f;

    Text(x, y, "Objects " + std::to_string(m_Editor.Objects().size()), TEXT_DIM);
    y -= ROW_H;
    Text(x, y, "Undo " + std::to_string(m_Editor.UndoCount()) + "  Redo " + std::to_string(m_Editor.RedoCount()), TEXT_DIM);
    y -= ROW_H;
    std::vector<std::string> issues = m_Editor.Validate();
    if (issues.empty())
        Text(x, y, "Playable", TEXT_DIM);
    else
    {
        Text(x, y, std::to_string(issues.size()) + " problem(s):", ERROR_TEXT);
        y -= ROW_H;
        Text(x, y, issues.front().substr(0, 22), ERROR_TEXT);
    }
}

void SceneEditorScene::RenderStatusBar()
{
    DrawPanel(0, 0, SCREEN_W, STATUS_H, PANEL_FILL, PANEL_BORDER);
    if (m_StatusTimer > 0.0f)
        Text(10.0f, 28.0f, m_Status, m_StatusIsError ? ERROR_TEXT : TEXT);
    const char* hints = m_Editor.IsPlaying()
                                ? "P stop  (the keyboard goes to the scene's scripts)"
                                : "WASD pan Z/C zoom 1-5 shape Space select R rotate F dup X del U/Y undo P play (click a value to type)";
    Text(10.0f, 8.0f, hints, TEXT_DIM);
}

void SceneEditorScene::DrawOutline(Entity entity, float r, float g, float b)
{
    std::vector<Vec3> outline = SceneObjects::WorldOutline(entity);
    for (size_t i = 0; i < outline.size(); ++i)
    {
        Vec2 a = m_Cam->WorldPointToScreenSpace(outline[i]);
        Vec2 c = m_Cam->WorldPointToScreenSpace(outline[(i + 1) % outline.size()]);
        App::DrawLine(a.X, a.Y, c.X, c.Y, r, g, b);
    }
}

void SceneEditorScene::RenderOverlay()
{
    if (m_Editor.IsPlaying())
        return;
    Entity selected = m_Editor.Selected();
    if (selected != NULL_ENTITY)
        DrawOutline(selected, ACCENT.R, ACCENT.G, ACCENT.B);

    if (MouseOverUI() || m_Dragging)
        return;
    Vec3 ground;
    if (!MouseToGround(ground))
        return;
    if (m_Tool == Tool::Place)
    {
        Vec3 snapped = m_Editor.ClampToField(SnapPoint(ground));
        Vec2 s = m_Cam->WorldPointToScreenSpace(snapped);
        App::DrawLine(s.X - 8, s.Y, s.X + 8, s.Y, 0.4f, 1.0f, 0.4f);
        App::DrawLine(s.X, s.Y - 8, s.X, s.Y + 8, 0.4f, 1.0f, 0.4f);
        Text(m_UI->mouseX + 14.0f, m_UI->mouseY + 10.0f, Editor::ObjectKindName(m_Kind));
    }
    else
    {
        Entity hovered = PickUnderMouse();
        if (hovered != NULL_ENTITY && hovered != selected && !m_Editor.IsField(hovered))
            DrawOutline(hovered, 0.8f, 0.8f, 0.8f);
    }
}
