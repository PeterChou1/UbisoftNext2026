#include "SceneEditorScene.h"

#include "AssetServer.h"
#include "Camera.h"
#include "ECSManager.h"
#include "EditorStyle.h"
#include "GameManager.h"
#include "GameOptions.h"
#include "Input.h"
#include "Lighting.h"
#include "Log.h"
#include "ModelImport.h"
#include "Scripting/ScriptRegistry.h"
#include "Transform.h"
#include "UIState.h"
#include "UIText.h"
#include "Widget.h"
#include "World/PhysicsGizmos.h"
#include "World/SceneComponents.h"
#include "stdafx.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

extern ECSManager ECS;
extern GameManager GameSceneManager;

using Editor::ObjectKind;
using SceneObjects::BodyType;
using namespace EditorStyle;

namespace
{
    // -- Camera ------------------------------------------------------------------------
    constexpr float PAN_SPEED = 18.0f;
    constexpr float ZOOM_SPEED = 25.0f;
    constexpr float MIN_DISTANCE = 6.0f;
    constexpr float MAX_DISTANCE = 90.0f;
    // Degrees per second of the arrow keys, units per second of E / V
    constexpr float ORBIT_SPEED = 90.0f;
    constexpr float TILT_SPEED = 45.0f;
    constexpr float MIN_PITCH = 10.0f;
    constexpr float MAX_PITCH = 89.0f;
    constexpr float RISE_SPEED = 8.0f;
    constexpr float MAX_VIEW_HEIGHT = 50.0f;
    // I / K raise / lower the selection by this much
    constexpr float HEIGHT_STEP = 0.5f;
    // Camera of the prefab stage
    constexpr float PREFAB_DISTANCE = 14.0f;

    constexpr float STATUS_TIME = 4.0f;
    constexpr float DRAG_THRESHOLD = 0.01f;
    constexpr float CROSS_SIZE = 0.5f;
    // Where "Create Child" puts the new object, from its parent
    const Vec3 CHILD_OFFSET = {1.0f, 0.0f, 0.0f};
    // Smallest scrollbar thumb
    constexpr float MIN_THUMB = 20.0f;
    // The Controls panel, over the scene view
    constexpr float CONTROLS_X = LEFT_W + 12.0f;
    constexpr float CONTROLS_W = SCREEN_W - LEFT_W - INSPECTOR_W - 24.0f;
    constexpr float CONTROLS_TOP = PANEL_TOP - 12.0f;
    constexpr float CONTROLS_BOTTOM = PANEL_BOTTOM + 12.0f;
    constexpr float CONTROLS_ROW = 17.0f;
    // Clicks this close (pixels) to a light's or a camera's marker pick it
    constexpr float MARKER_PICK_PIXELS = 12.0f;

    // Text fields that edit the selected object or the scene settings
    bool InObjectFields(int id)
    {
        return (id >= ID_FIELD_FIRST_OBJECT && id <= ID_FIELD_SCENE_PARAM + MAX_PARAM_FIELDS) ||
               (id >= ID_FIELD_FIRST_COMPONENT && id <= ID_FIELD_LAST_COMPONENT);
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
    // The light follows the scene's light object (GameManager, SceneLight.h)
    SceneLighting::Update(*m_Light, *m_Options);
    m_Cam->SetProjectionPerspective();

    m_View = DefaultView();
    m_Dragging = false;
    m_AssetDrag = false;
    m_PlaceKind = AssetKind::None;
    m_PlaceName.clear();
    m_PendingSceneIndex = -1;
    m_PrefabMode = false;
    m_ShowScene = false;
    m_Menu.Close();
    m_UI->openDropDownId = 0;
    m_UI->focusedItem = 0;
    std::error_code importDirError;
    std::filesystem::create_directories(IMPORT_DIRECTORY, importDirError);
    RefreshAssets();

    // Open the first scene of the folder, or start a new one
    RefreshSceneList();
    if (m_SceneNames.empty() || !OpenScene(m_SceneNames.front()))
        NewDocument();
    SetStatus("Right click the scene or the hierarchy to create objects. Tutorial: docs/EditorTutorial.md");
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
    if (Input::WasPressed(App::KEY_P) && !m_PrefabMode)
        TogglePlay();
    if (m_Editor.IsPlaying())
    {
        // The game camera (scripts may move it, or drive the camera themselves)
        m_PlayCamera.Update(*m_Cam);
        return;
    }

    UpdateCamera(deltaSeconds);
    UpdateShortcuts();
    UpdateViewportMouse();
}

void SceneEditorScene::Render()
{
    m_NextId = FIRST_DYNAMIC_ID;
    m_NextFieldId = ID_FIELD_FIRST_COMPONENT;
    m_Hint.clear();
    m_DrawnFields.clear();

    // While the scene list or a context menu is open it covers the other
    // widgets: they must not receive the click meant for it
    bool menuOpen = m_Menu.IsOpen();
    bool listOpen = m_UI->openDropDownId == ID_SCENE_LIST;
    bool click = m_UI->leftClick;
    bool rightClick = m_UI->rightClick;
    if (listOpen || menuOpen)
        m_UI->leftClick = false;
    if (menuOpen)
        m_UI->rightClick = false;

    // The inspector's text fields always edit the selected object: an edit
    // in progress is dropped when the selection changes
    if (m_Editor.Selected() != m_FieldsEntity)
    {
        if (InObjectFields(m_UI->focusedItem))
            m_UI->focusedItem = 0;
        m_FieldsEntity = m_Editor.Selected();
        m_AddIndex = 0;
        m_InspectorScroll = 0.0f;
    }

    RenderOverlay();
    if (m_ShowControls)
        RenderControlsPanel();
    RenderLeftPanel();
    RenderInspector();
    RenderStatusBar();
    if (RenderToolbar())
    {
        m_UI->leftClick = menuOpen ? false : click;
        if (!m_Editor.IsPlaying() && !m_PrefabMode)
            RenderSceneList();
        if (listOpen && click && m_UI->openDropDownId == ID_SCENE_LIST && m_UI->activeItem != ID_SCENE_LIST)
            m_UI->openDropDownId = 0;
    }

    // A text field scrolled out of view (or hidden) can not finish its edit:
    // drop the edit instead of leaving the keyboard captured
    if (InObjectFields(m_UI->focusedItem) && m_DrawnFields.count(m_UI->focusedItem) == 0)
        m_UI->focusedItem = 0;

    // The context menu last, on top of everything, with the real clicks
    m_UI->leftClick = click;
    m_UI->rightClick = rightClick;
    m_Menu.Render(*m_UI);
}

//-----------------------------------------------------------------------------
// Input
//-----------------------------------------------------------------------------

SceneCamera::View SceneEditorScene::DefaultView()
{
    // Where the scenes' camera used to be: looking at the field's centre
    SceneCamera::View view;
    view.Distance = 30.0f;
    return view;
}

void SceneEditorScene::SetEditorView(const SceneCamera::View& view)
{
    m_View = view;
    m_View.Distance = std::clamp(m_View.Distance, MIN_DISTANCE, MAX_DISTANCE);
    m_View.Pitch = std::clamp(m_View.Pitch, MIN_PITCH, MAX_PITCH);
    m_View.Target.Y = std::clamp(m_View.Target.Y, -MAX_VIEW_HEIGHT, MAX_VIEW_HEIGHT);
    m_View.Yaw = std::fmod(m_View.Yaw, 360.0f);
    if (m_View.Yaw < 0.0f)
        m_View.Yaw += 360.0f;
    // The editor always looks with the default lens
    m_View.FieldOfView = 90.0f;
    SceneCamera::Apply(*m_Cam, m_View);
}

void SceneEditorScene::UpdateCamera(float deltaSeconds)
{
    SceneCamera::View view = m_View;
    // Pan along the ground, relative to where the view looks
    float pan = PAN_SPEED * deltaSeconds * (view.Distance / 30.0f);
    Vec3 forward = SceneCamera::Forward(view.Yaw) * pan;
    Vec3 right = SceneCamera::Right(view.Yaw) * pan;
    if (Input::IsDown(App::KEY_W))
        view.Target = view.Target + forward;
    if (Input::IsDown(App::KEY_S))
        view.Target = view.Target - forward;
    if (Input::IsDown(App::KEY_A))
        view.Target = view.Target - right;
    if (Input::IsDown(App::KEY_D))
        view.Target = view.Target + right;
    // Orbit around the target and tilt
    if (Input::IsDown(App::KEY_LEFT))
        view.Yaw -= ORBIT_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_RIGHT))
        view.Yaw += ORBIT_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_UP))
        view.Pitch += TILT_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_DOWN))
        view.Pitch -= TILT_SPEED * deltaSeconds;
    // Up / down (Q is the framework's quit key)
    if (Input::IsDown(App::KEY_E))
        view.Target.Y += RISE_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_V))
        view.Target.Y -= RISE_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_Z))
        view.Distance -= ZOOM_SPEED * deltaSeconds;
    if (Input::IsDown(App::KEY_C))
        view.Distance += ZOOM_SPEED * deltaSeconds;
    if (Input::WasPressed(App::KEY_HOME))
    {
        view = DefaultView();
        if (m_PrefabMode)
            view.Distance = PREFAB_DISTANCE;
        SetStatus("View reset (Home)");
    }
    SetEditorView(view);
}

void SceneEditorScene::UpdateShortcuts()
{
    if (m_Menu.IsOpen())
        return;
    // Esc stops placing an asset
    for (char c : Input::TypedText())
    {
        if (c == 27 && m_ShowControls)
        {
            m_ShowControls = false;
            continue;
        }
        if (c == 27 && m_PlaceKind != AssetKind::None)
        {
            m_PlaceKind = AssetKind::None;
            m_PlaceName.clear();
            SetStatus("Stopped placing");
        }
    }
    if (Input::WasPressed(App::KEY_R) || Input::WasPressed(App::KEY_L))
        RotateSelected(ROTATE_STEP);
    if (Input::WasPressed(App::KEY_J))
        RotateSelected(-ROTATE_STEP);
    if (Input::WasPressed(App::KEY_I))
        RaiseSelected(HEIGHT_STEP);
    if (Input::WasPressed(App::KEY_K))
        RaiseSelected(-HEIGHT_STEP);
    if (Input::WasPressed(App::KEY_H))
        ToggleControls();
    if (Input::WasPressed(App::KEY_X))
        DeleteSelected();
    if (Input::WasPressed(App::KEY_F))
        DuplicateSelected();
    if (Input::WasPressed(App::KEY_U) && m_Editor.Undo())
        SetStatus("Undo");
    if (Input::WasPressed(App::KEY_Y) && m_Editor.Redo())
        SetStatus("Redo");
    if (Input::WasPressed(App::KEY_B))
    {
        ToggleColliders();
        SetStatus(m_ShowColliders ? "Showing every collider (B)" : "Showing the selected collider (B)");
    }
    if (Input::WasPressed(App::KEY_G))
    {
        m_Snap = !m_Snap;
        SetStatus(m_Snap ? "Snap to grid on (G)" : "Snap to grid off (G)");
    }
}

bool SceneEditorScene::MouseOverUI() const
{
    float x = m_UI->mouseX;
    float y = m_UI->mouseY;
    return m_UI->openDropDownId != 0 || m_Menu.Contains(x, y) || x < LEFT_W || x > SCREEN_W - INSPECTOR_W ||
           y > SCREEN_H - TOOLBAR_H || y < STATUS_H ||
           (m_ShowControls && Inside(x, y, CONTROLS_X, CONTROLS_BOTTOM, CONTROLS_W, CONTROLS_TOP - CONTROLS_BOTTOM));
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
    // Markers drawn in the air (a light's sun, a camera's eye) are picked by
    // their distance on the screen: at a grazing view one pixel is a long
    // way along a horizontal plane
    Entity marker = NULL_ENTITY;
    float markerHeight = 0.0f;
    float best = MARKER_PICK_PIXELS * MARKER_PICK_PIXELS;
    Vec3 forward = m_Cam->Backward * -1.0f;
    for (Entity e : m_Editor.Objects())
    {
        Vec3 point;
        if (m_Editor.IsLight(e))
            point = SceneObjects::GetPosition(e);
        else if (m_Editor.IsCamera(e))
            point = SceneCamera::EyeOf(SceneCamera::ViewOf(e));
        else
            continue;
        if ((point - m_Cam->Position).Dot(forward) <= 0.5f)
            continue;
        Vec2 s = m_Cam->WorldPointToScreenSpace(point);
        float dx = s.X - m_UI->mouseX;
        float dy = s.Y - m_UI->mouseY;
        if (dx * dx + dy * dy <= best)
        {
            best = dx * dx + dy * dy;
            marker = e;
            markerHeight = point.Y;
        }
    }
    if (marker != NULL_ENTITY)
    {
        if (hitHeight != nullptr)
            *hitHeight = markerHeight;
        return marker;
    }
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
    return {Editor::SceneEditor::Snap(point.X, SNAP_STEP), point.Y, Editor::SceneEditor::Snap(point.Z, SNAP_STEP)};
}

Vec3 SceneEditorScene::ViewCenter() const
{
    Vec3 center = m_Editor.ClampToField(SnapPoint(m_View.Target));
    center.Y = 0.0f;
    return center;
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
    // The context menu handles the mouse while it is open
    if (m_Menu.IsOpen())
        return;

    // An asset dragged from the Assets list is dropped where it is released
    if (m_AssetDrag && !m_UI->mouseLeftDown)
    {
        m_AssetDrag = false;
        Vec3 ground;
        if (!MouseOverUI() && MouseToGround(ground))
            PlaceAsset(SnapPoint(ground));
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

    if (MouseOverUI() || m_AssetDrag)
        return;
    // The first click outside a text field only finishes the edit
    if (m_UI->IsTyping())
        return;

    if (m_UI->rightClick)
    {
        if (m_PlaceKind != AssetKind::None)
        {
            m_PlaceKind = AssetKind::None;
            m_PlaceName.clear();
            SetStatus("Stopped placing");
            return;
        }
        OpenViewportMenu();
        return;
    }
    if (!m_UI->leftClick)
        return;

    Vec3 ground;
    if (m_PlaceKind != AssetKind::None)
    {
        if (MouseToGround(ground))
            PlaceAsset(SnapPoint(ground));
        return;
    }

    // Pressing an object selects and grabs it
    float grabHeight = 0.0f;
    Entity picked = PickUnderMouse(&grabHeight);
    if (picked != NULL_ENTITY && !m_Editor.IsField(picked))
    {
        m_Editor.Select(picked);
        m_DragRecorded = false;
        BeginDrag(picked, grabHeight);
        return;
    }
    // Clicking the field selects it (to resize / recolour it)
    m_Editor.Select(picked);
}

void SceneEditorScene::OpenViewportMenu()
{
    Entity picked = PickUnderMouse();
    if (picked != NULL_ENTITY && !m_Editor.IsField(picked))
    {
        m_Editor.Select(picked);
        m_Menu.Open(m_UI->mouseX, m_UI->mouseY, ObjectItems(picked));
        return;
    }
    // On the field (or nothing): create objects where it was clicked
    Vec3 ground;
    Vec3 at = MouseToGround(ground) ? m_Editor.ClampToField(SnapPoint(ground)) : ViewCenter();
    at.Y = 0.0f;
    m_Menu.Open(m_UI->mouseX, m_UI->mouseY, CreateItems(at, NULL_ENTITY));
}

//-----------------------------------------------------------------------------
// Context menus
//-----------------------------------------------------------------------------

std::vector<MenuItem> SceneEditorScene::CreateItems(const Vec3& position, Entity parent)
{
    std::vector<MenuItem> items;
    for (ObjectKind kind :
         {ObjectKind::Empty, ObjectKind::Rectangle, ObjectKind::Circle, ObjectKind::Triangle, ObjectKind::Polygon})
    {
        items.push_back({std::string("Create ") + Editor::ObjectKindName(kind),
                         [this, kind, position, parent] { CreateObject(kind, position, parent); }});
    }
    MenuItem models{"Create Model", nullptr};
    for (const std::string& model : m_Models)
    {
        models.Children.push_back(
                {model, [this, position, parent, model] { CreateObject(ObjectKind::Model, position, parent, model); }});
    }
    models.Enabled = !models.Children.empty();
    items.push_back(models);

    MenuItem prefabs{"Create Prefab", nullptr};
    for (const std::string& name : m_Prefabs)
    {
        prefabs.Children.push_back({name, [this, position, parent, name] {
                                        Prefab::Data data;
                                        std::string error;
                                        if (!Prefab::LoadFile(Prefab::PathOf(name, m_PrefabDirectory), data, error))
                                        {
                                            SetStatus("Can not load prefab " + name + ": " + error, true);
                                            return;
                                        }
                                        Entity e = m_Editor.PlacePrefab(data, position, parent);
                                        if (e != NULL_ENTITY)
                                            SetStatus("Placed prefab " + name + " (" + m_Editor.NameOf(e) + ")");
                                    }});
    }
    prefabs.Enabled = !prefabs.Children.empty();
    items.push_back(prefabs);
    // A scene's game camera and light (top level only; prefabs have none)
    if (parent == NULL_ENTITY && !m_PrefabMode)
    {
        items.push_back({"Create Light", [this, position] {
                             Entity e = m_Editor.AddLight(position);
                             SetStatus("Created " + m_Editor.NameOf(e) +
                                       (m_Editor.LightObject() == e ? " (the scene's light)"
                                                                    : " (the scene uses the first light)"));
                         }});
        items.push_back({"Create Camera", [this, position] {
                             Entity e = m_Editor.AddCamera(position);
                             SetStatus("Created " + m_Editor.NameOf(e) +
                                       (m_Editor.GameCameraObject() == e ? " (the game camera)"
                                                                         : " (the game uses the first camera)"));
                         }});
    }
    return items;
}

std::vector<MenuItem> SceneEditorScene::ObjectItems(Entity e)
{
    // The field only offers to create objects
    if (m_Editor.IsField(e))
        return CreateItems(ViewCenter(), NULL_ENTITY);

    std::vector<MenuItem> items;
    Vec3 childAt = m_Editor.ClampToField(SceneObjects::GetPosition(e) + CHILD_OFFSET);
    childAt.Y = 0.0f;
    MenuItem child{"Create Child", nullptr};
    child.Children = CreateItems(childAt, e);
    items.push_back(child);
    items.push_back({"Rename", [this, e] {
                         // Starts typing in the inspector's Name box
                         m_Editor.Select(e);
                         m_FieldsEntity = e;
                         m_InspectorScroll = 0.0f;
                         m_ShowScene = false;
                         m_UI->focusedItem = ID_FIELD_NAME;
                         m_UI->editText = m_Editor.NameOf(e);
                         m_UI->editFresh = true;
                     }});
    items.push_back({"Duplicate", [this, e] {
                         m_Editor.Select(e);
                         DuplicateSelected();
                     }});
    items.push_back({"Delete", [this, e] {
                         m_Editor.Select(e);
                         DeleteSelected();
                     }});
    if (m_Editor.ParentOf(e) != NULL_ENTITY)
    {
        items.push_back({"Unparent", [this, e] {
                             if (m_Editor.SetParent(e, NULL_ENTITY))
                                 SetStatus(m_Editor.NameOf(e) + " is now a top level object");
                         }});
    }
    items.push_back({"Focus", [this, e] {
                         SceneCamera::View view = m_View;
                         view.Target = SceneObjects::GetPosition(e);
                         SetEditorView(view);
                     }});
    if (m_Editor.IsLight(e))
    {
        items.push_back({"Aim at View Center", [this, e] {
                             if (m_Editor.AimLight(e, ViewCenter()))
                                 SetStatus(m_Editor.NameOf(e) + " now shines at the centre of the view");
                         }});
    }
    else if (!m_Editor.IsCamera(e) && m_Editor.LightObject() != NULL_ENTITY)
    {
        // Point the scene's light at this object
        items.push_back({"Aim Light Here", [this, e] {
                             Entity light = m_Editor.LightObject();
                             if (m_Editor.AimLight(light, SceneObjects::GetPosition(e)))
                                 SetStatus(m_Editor.NameOf(light) + " now shines at " + m_Editor.NameOf(e));
                         }});
    }
    if (m_Editor.IsCamera(e))
    {
        // Game camera <-> the editor's view
        items.push_back({"Align with View", [this, e] {
                             SceneCamera::View view = m_View;
                             view.FieldOfView = ECS.GetComponent<GameCamera>(e).FieldOfView;
                             m_Editor.SetCameraView(e, view);
                             SetStatus(m_Editor.NameOf(e) + " now shows the current view");
                         }});
        items.push_back({"View Through Camera", [this, e] {
                             SetEditorView(SceneCamera::ViewOf(e));
                             SetStatus("The view shows what " + m_Editor.NameOf(e) + " sees");
                         }});
    }
    items.push_back({"Save as Prefab", [this, e] { SaveAsPrefab(e); }});
    std::string prefab = m_Editor.PrefabOf(e);
    if (!prefab.empty())
    {
        items.push_back({"Edit Prefab", [this, prefab] { EditPrefab(prefab); }});
        items.push_back({"Reset to Prefab", [this, e, prefab] {
                             Prefab::Data data;
                             std::string error;
                             if (!Prefab::LoadFile(Prefab::PathOf(prefab, m_PrefabDirectory), data, error))
                             {
                                 SetStatus("Can not load prefab " + prefab + ": " + error, true);
                                 return;
                             }
                             if (m_Editor.ResetToPrefab(e, data) != NULL_ENTITY)
                                 SetStatus("Reset to prefab " + prefab);
                         }});
        items.push_back({"Unpack Prefab", [this, e, prefab] {
                             if (m_Editor.UnpackPrefab(e))
                                 SetStatus(m_Editor.NameOf(e) + " is no longer linked to " + prefab);
                         }});
    }
    return items;
}

//-----------------------------------------------------------------------------
// Actions
//-----------------------------------------------------------------------------

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

void SceneEditorScene::RaiseSelected(float amount)
{
    Entity selected = m_Editor.Selected();
    if (selected == NULL_ENTITY)
        return;
    float y = SceneObjects::GetPosition(selected).Y + amount;
    if (m_Editor.SetHeight(selected, y))
        SetStatus(m_Editor.NameOf(selected) + " height " + Fmt("%.2f", SceneObjects::GetPosition(selected).Y));
}

void SceneEditorScene::CreateObject(ObjectKind kind, const Vec3& position, Entity parent, const std::string& model)
{
    Editor::PlaceSettings settings;
    settings.Model = model;
    Entity e = m_Editor.Create(kind, position, parent, settings);
    if (e == NULL_ENTITY)
    {
        SetStatus(std::string("Can not create ") + Editor::ObjectKindName(kind), true);
        return;
    }
    m_ShowScene = false;
    SetStatus("Created " + m_Editor.NameOf(e) + (parent != NULL_ENTITY ? " in " + m_Editor.NameOf(parent) : ""));
}

void SceneEditorScene::StartPlacingPrefab(const std::string& name)
{
    m_PlaceKind = name.empty() ? AssetKind::None : AssetKind::Prefab;
    m_PlaceName = name;
    if (!name.empty())
        SetStatus("Placing prefab " + name + ": click the scene (right click or Esc to stop)");
}

void SceneEditorScene::StartPlacingModel(const std::string& name)
{
    m_PlaceKind = name.empty() ? AssetKind::None : AssetKind::Model;
    m_PlaceName = name;
    if (!name.empty())
        SetStatus("Placing model " + name + ": click the scene (right click or Esc to stop)");
}

void SceneEditorScene::PlaceAsset(const Vec3& position)
{
    Entity placed = NULL_ENTITY;
    if (m_PlaceKind == AssetKind::Prefab)
    {
        Prefab::Data data;
        std::string error;
        if (!Prefab::LoadFile(Prefab::PathOf(m_PlaceName, m_PrefabDirectory), data, error))
        {
            SetStatus("Can not load prefab " + m_PlaceName + ": " + error, true);
            m_PlaceKind = AssetKind::None;
            return;
        }
        placed = m_Editor.PlacePrefab(data, position);
    }
    else if (m_PlaceKind == AssetKind::Model)
    {
        Editor::PlaceSettings settings;
        settings.Model = m_PlaceName;
        placed = m_Editor.Create(ObjectKind::Model, position, NULL_ENTITY, settings);
    }
    if (placed == NULL_ENTITY)
        return;
    m_ShowScene = false;
    SetStatus("Placed " + m_Editor.NameOf(placed) + " (keep the button down to drag it)");
    // Place and drag in one gesture; placing already recorded the undo step
    m_DragRecorded = true;
    if (m_UI->mouseLeftDown)
        BeginDrag(placed, 0.0f);
}

void SceneEditorScene::TogglePlay()
{
    if (m_Editor.IsPlaying())
    {
        // Stop: drop the running scripts, then restore the authored world
        GameSceneManager.Scripts().Reset();
        m_Editor.EndPlay();
        SetEditorView(m_View);
        SetStatus("Stopped, the scene is back to how it was before Play");
        return;
    }
    if (m_PrefabMode)
        return;
    std::vector<std::string> issues = m_Editor.Validate();
    if (!issues.empty())
    {
        SetStatus("Can not play: " + issues.front(), true);
        return;
    }
    m_Dragging = false;
    m_AssetDrag = false;
    m_Menu.Close();
    m_UI->openDropDownId = 0;
    m_UI->focusedItem = 0;
    m_Editor.BeginPlay();
    GameSceneManager.Scripts().Reset();
    // Play shows the scene's game camera; Stop goes back to the editor's view
    m_PlayCamera.Reset();
    m_PlayCamera.Update(*m_Cam);
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
    if (m_PrefabMode)
        return;
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
    if (m_PrefabMode)
    {
        SetStatus("Go back to the scene before opening another one", true);
        return false;
    }
    Serialization::LoadResult result = m_Editor.LoadScene(ScenePath(name));
    if (!result)
    {
        SetStatus("Could not open " + name + ": " + result.Error, true);
        m_SceneIndex = IndexOf(m_SceneNames, m_DocName);
        return false;
    }
    m_DocName = name;
    m_PendingSceneIndex = -1;
    m_Collapsed.clear();
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
    if (m_PrefabMode)
        return SavePrefab();
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
    if (newName.empty() || newName == m_DocName || m_PrefabMode)
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
// Prefabs
//-----------------------------------------------------------------------------

void SceneEditorScene::SetPrefabDirectory(const std::string& directory)
{
    m_PrefabDirectory = directory;
    RefreshAssets();
}

void SceneEditorScene::RefreshAssets()
{
    m_Prefabs = Prefab::Available(m_PrefabDirectory);
    m_Models = AssetServer::AvailableModels();
}

std::string SceneEditorScene::UniquePrefabName(const std::string& base) const
{
    std::string safe = Prefab::SafeName(base);
    std::error_code ec;
    if (!std::filesystem::exists(Prefab::PathOf(safe, m_PrefabDirectory), ec))
        return safe;
    for (int i = 2;; ++i)
    {
        std::string name = safe + "_" + std::to_string(i);
        if (!std::filesystem::exists(Prefab::PathOf(name, m_PrefabDirectory), ec))
            return name;
    }
}

bool SceneEditorScene::SaveAsPrefab(Entity root)
{
    if (!m_Editor.IsObject(root) || m_Editor.IsField(root))
    {
        SetStatus("Select an object (not the field) to save it as a prefab", true);
        return false;
    }
    std::string name = UniquePrefabName(m_Editor.NameOf(root));
    Prefab::Data data = m_Editor.CapturePrefab(root, name);
    std::string error;
    if (!Prefab::SaveFile(Prefab::PathOf(name, m_PrefabDirectory), data, error))
    {
        SetStatus("Could not save prefab " + name + ": " + error, true);
        return false;
    }
    // In the prefab editor the group stays plain objects of the stage
    if (!m_PrefabMode)
        m_Editor.LinkPrefab(root, name);
    RefreshAssets();
    SetStatus("Saved prefab " + name + " (" + std::to_string(data.Objects.size()) + " objects). Place it from Assets");
    return true;
}

bool SceneEditorScene::EditPrefab(const std::string& name)
{
    if (m_Editor.IsPlaying())
        return false;
    if (m_PrefabMode)
    {
        if (name != m_PrefabName)
            SetStatus("Finish editing " + m_PrefabName + " first (Back to Scene)", true);
        return false;
    }
    Prefab::Data data;
    std::string error;
    std::vector<std::string> warnings;
    if (!Prefab::LoadFile(Prefab::PathOf(name, m_PrefabDirectory), data, error, &warnings))
    {
        SetStatus("Can not open prefab " + name + ": " + error, true);
        return false;
    }
    m_Session = m_Editor.Suspend();
    m_PrefabMode = true;
    m_PrefabName = name;
    m_PrefabSaved = false;
    m_ExitPending = false;
    m_ShowScene = false;
    m_Menu.Close();
    m_PlaceKind = AssetKind::None;
    m_UI->focusedItem = 0;
    m_Editor.OpenPrefabStage(&data, name);
    m_SceneView = m_View;
    m_View = DefaultView();
    m_View.Distance = PREFAB_DISTANCE;
    SetStatus("Editing prefab " + name + ": Save Prefab, then Back to Scene" +
              (warnings.empty() ? std::string() : " (" + warnings.front() + ")"));
    return true;
}

bool SceneEditorScene::NewPrefab()
{
    if (m_Editor.IsPlaying() || m_PrefabMode)
        return false;
    std::string name = UniquePrefabName("prefab");
    m_Session = m_Editor.Suspend();
    m_PrefabMode = true;
    m_PrefabName = name;
    m_PrefabSaved = false;
    m_ExitPending = false;
    m_ShowScene = false;
    m_Menu.Close();
    m_PlaceKind = AssetKind::None;
    m_UI->focusedItem = 0;
    m_Editor.OpenPrefabStage(nullptr, name);
    m_SceneView = m_View;
    m_View = DefaultView();
    m_View.Distance = PREFAB_DISTANCE;
    SetStatus("New prefab " + name + ": right click " + name + " to add objects, then Save Prefab");
    return true;
}

bool SceneEditorScene::SavePrefab()
{
    if (!m_PrefabMode)
        return false;
    Prefab::Data data = m_Editor.CaptureStage(m_PrefabName);
    std::string error;
    if (!Prefab::SaveFile(Prefab::PathOf(m_PrefabName, m_PrefabDirectory), data, error))
    {
        SetStatus("Could not save prefab: " + error, true);
        return false;
    }
    m_Editor.MarkSaved();
    m_PrefabSaved = true;
    m_ExitPending = false;
    RefreshAssets();
    SetStatus("Saved prefab " + m_PrefabName + " (" + std::to_string(data.Objects.size()) + " objects)");
    return true;
}

bool SceneEditorScene::BackToScene()
{
    if (!m_PrefabMode)
        return false;
    if (m_Editor.IsDirty() && !m_ExitPending)
    {
        m_ExitPending = true;
        SetStatus(m_PrefabName + " has unsaved changes: Save Prefab, or Back to Scene again to discard them", true);
        return false;
    }
    m_Menu.Close();
    m_UI->focusedItem = 0;
    m_PlaceKind = AssetKind::None;
    m_Editor.Resume(m_Session);
    m_Session = Editor::SceneEditor::Session{};
    m_View = m_SceneView;
    m_PrefabMode = false;
    std::string name = m_PrefabName;
    if (!m_PrefabSaved)
    {
        SetStatus("Back to " + m_DocName);
        return true;
    }
    // The scene's instances follow the saved prefab
    Prefab::Data data;
    std::string error;
    if (!Prefab::LoadFile(Prefab::PathOf(name, m_PrefabDirectory), data, error))
    {
        SetStatus("Back to " + m_DocName + ", but " + name + " can not be read: " + error, true);
        return true;
    }
    int updated = m_Editor.UpdatePrefabInstances(data);
    SetStatus("Back to " + m_DocName +
              (updated > 0 ? ": updated " + std::to_string(updated) + " instance(s) of " + name : std::string()));
    return true;
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
    RefreshAssets();
    StartPlacingModel(result.Name);
    std::string message = (result.AlreadyImported ? "Already imported: " : "Imported ") + result.Name + " (" +
                          std::to_string(result.Triangles) + " triangles). Click the scene to place it";
    if (!result.Warnings.empty())
        message += ". " + result.Warnings.front();
    SetStatus(message, false);
    m_ImportPath.clear();
}

//-----------------------------------------------------------------------------
// Widgets
//-----------------------------------------------------------------------------

int SceneEditorScene::Stepper(
        float x, float y, float width, const std::string& label, const char* minus, const char* plus)
{
    // "label        [-] [+]" row, returns -1 / +1 when a button was clicked.
    // The label is cut before the buttons
    Text(x, RowY(y), label, TEXT, width - 2 * STEP_W - 10.0f);
    float right = x + width;
    int delta = 0;
    if (Button(NextId(), right - 2 * STEP_W - 4.0f, y, *m_UI, STEP_W, WIDGET_H, minus))
        delta = -1;
    if (Button(NextId(), right - STEP_W, y, *m_UI, STEP_W, WIDGET_H, plus))
        delta = 1;
    return delta;
}

bool SceneEditorScene::NumberRow(
        float x, float y, float width, const std::string& label, int fieldId, float& value, float step, const char* format)
{
    // "label [ value ] [-] [+]": type a value (Enter) or step it
    Text(x, RowY(y), label, TEXT, LABEL_W - 4.0f);
    float fieldX = x + LABEL_W;
    float fieldW = width - LABEL_W - 2 * STEP_W - 10.0f;
    bool changed = false;
    std::string text = Fmt(format, value);
    FieldDrawn(fieldId);
    if (TextField(fieldId, fieldX, y, fieldW, WIDGET_H, *m_UI, text, TextFilter::Number) == TextFieldEvent::Committed)
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
    if (Button(NextId(), right - 2 * STEP_W - 4.0f, y, *m_UI, STEP_W, WIDGET_H, "-"))
    {
        value -= step;
        changed = true;
    }
    if (Button(NextId(), right - STEP_W, y, *m_UI, STEP_W, WIDGET_H, "+"))
    {
        value += step;
        changed = true;
    }
    return changed;
}

void SceneEditorScene::Scrollbar(int id, float x, float bottom, float top, float content, float& scroll)
{
    float view = top - bottom;
    float range = content - view;
    if (range <= 0.0f)
    {
        scroll = 0.0f;
        if (m_ScrollDragId == id)
            m_ScrollDragId = 0;
        return;
    }
    scroll = std::clamp(scroll, 0.0f, range);
    float thumbH = std::max(MIN_THUMB, view * view / content);
    auto thumbTopFor = [&](float s) { return top - (s / range) * (view - thumbH); };
    float thumbTop = thumbTopFor(scroll);

    float mx = m_UI->mouseX;
    float my = m_UI->mouseY;
    bool overTrack = Inside(mx, my, x, bottom, SCROLLBAR_W, view);
    bool overThumb = Inside(mx, my, x, thumbTop - thumbH, SCROLLBAR_W, thumbH);
    if (m_UI->leftClick && overThumb)
    {
        m_ScrollDragId = id;
        m_ScrollGrab = thumbTop - my;
    }
    else if (m_UI->leftClick && overTrack)
    {
        // Page up / down towards the click
        scroll = std::clamp(scroll + (my > thumbTop ? -view : view) * 0.9f, 0.0f, range);
    }
    if (m_ScrollDragId == id)
    {
        if (!m_UI->mouseLeftDown)
            m_ScrollDragId = 0;
        else
        {
            float newTop = std::clamp(my + m_ScrollGrab, bottom + thumbH, top);
            scroll = std::clamp((top - newTop) / (view - thumbH) * range, 0.0f, range);
        }
    }
    thumbTop = thumbTopFor(scroll);
    DrawPanel(x, bottom, SCROLLBAR_W, view, PANEL_FILL, PANEL_BORDER);
    const Color& thumb = m_ScrollDragId == id || overThumb ? ACCENT : TEXT_DIM;
    DrawPanel(x + 1.0f, thumbTop - thumbH, SCROLLBAR_W - 2.0f, thumbH, thumb, thumb);
}

int SceneEditorScene::NextFieldId()
{
    // Past the range, fields share the last id (never reached in practice)
    return m_NextFieldId <= ID_FIELD_LAST_COMPONENT ? m_NextFieldId++ : ID_FIELD_LAST_COMPONENT;
}

//-----------------------------------------------------------------------------
// Toolbar, status bar, scene view overlay
//-----------------------------------------------------------------------------

bool SceneEditorScene::RenderToolbar()
{
    float y = SCREEN_H - TOOLBAR_H;
    DrawPanel(0, y, SCREEN_W, TOOLBAR_H, PANEL_FILL, PANEL_BORDER);
    float by = y + (TOOLBAR_H - BUTTON_H) * 0.5f;
    float x = LEFT_W;
    bool playing = m_Editor.IsPlaying();

    // Buttons are as wide as their label needs (at least `width`)
    auto toolbarButton = [&](const std::string& label, float width) {
        float w = std::max(width, UIText::Width(label) + 16.0f);
        bool clicked = Button(NextId(), x, by, *m_UI, w, BUTTON_H, label);
        x += w + 8.0f;
        return clicked;
    };

    if (m_PrefabMode)
    {
        Text(10.0f, RowY(by), "PREFAB " + m_PrefabName, PREFAB_TEXT, LEFT_W - 20.0f);
        if (toolbarButton("Save Prefab", 90))
            SavePrefab();
        if (toolbarButton("Undo", 50) && !m_Editor.Undo())
            SetStatus("Nothing to undo");
        if (toolbarButton("Redo", 50) && !m_Editor.Redo())
            SetStatus("Nothing to redo");
        if (toolbarButton("Back to Scene", 110))
            BackToScene();

        if (m_Editor.IsDirty())
            Text(x + 4.0f, RowY(by), "*", ACCENT);
        return true;
    }

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
    if (toolbarButton(playing ? "Stop" : "Play", 60))
        TogglePlay();
    if (!playing && toolbarButton(m_ShowScene ? "Object" : "Scene", 70))
        m_ShowScene = !m_ShowScene;

    if (playing)
    {
        Text(x + 20.0f, RowY(by), "PLAYING " + m_DocName, PLAY_TEXT);
        return true;
    }
    // The scene's name: click it to rename the scene (and its file)
    float labelW = UIText::Width("Name");
    Text(x + 4.0f, RowY(by), "Name", TEXT_DIM, labelW + 1.0f);
    float fieldX = x + 12.0f + labelW;
    float fieldW = std::max(60.0f, std::min(150.0f, SCREEN_W - fieldX - 24.0f));
    std::string name = m_DocName;
    if (TextField(ID_FIELD_SCENE_NAME, fieldX, by + 2.0f, fieldW, WIDGET_H, *m_UI, name, TextFilter::Name) ==
        TextFieldEvent::Committed)
        RenameDocument(name);
    if (m_Editor.IsDirty())
        Text(fieldX + fieldW + 6.0f, RowY(by), "*", ACCENT);
    return true;
}

void SceneEditorScene::RenderSceneList()
{
    int before = m_SceneIndex;
    if (!DropdownList(ID_SCENE_LIST,
                      10.0f,
                      SCREEN_H - TOOLBAR_H + (TOOLBAR_H - BUTTON_H) * 0.5f,
                      LEFT_W - 20.0f,
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

void SceneEditorScene::RenderStatusBar()
{
    DrawPanel(0, 0, SCREEN_W, STATUS_H, PANEL_FILL, PANEL_BORDER);
    // Right end: the Controls panel (also H)
    float buttonW = std::max(80.0f, UIText::Width("Controls") + 16.0f);
    float buttonX = SCREEN_W - buttonW - 10.0f;
    if (Button(NextId(), buttonX, (STATUS_H - BUTTON_H) * 0.5f, *m_UI, buttonW, BUTTON_H, "Controls"))
        ToggleControls();
    float room = buttonX - 20.0f;
    if (m_StatusTimer > 0.0f)
        Text(10.0f, 28.0f, m_Status, m_StatusIsError ? ERROR_TEXT : TEXT, room);
    const char* hints =
            m_Editor.IsPlaying()
                    ? "P stop  (the keyboard goes to the scene's scripts)"
                    : "H: all controls  Right click: menus  WASD pan  arrows orbit/tilt  E/V up/down  Z/C zoom  I/K raise/lower  P play";
    // The tooltip of the field under the mouse replaces the hints
    if (!m_Hint.empty())
        Text(10.0f, 8.0f, m_Hint, ACCENT, room);
    else
        Text(10.0f, 8.0f, hints, TEXT_DIM, room);
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

void SceneEditorScene::DrawCross(Entity entity, const Color& color, float size)
{
    // An empty is a cross on the ground turned with the object, and a short
    // post showing its height
    Transform world = ECS.GetComponent<Transform>(entity).GetWorldTransform();
    Vec3 c = world.LocalPosition;
    Vec3 right = world.LocalRotation.RotatePoint(Vec3(size, 0.0f, 0.0f));
    Vec3 forward = world.LocalRotation.RotatePoint(Vec3(0.0f, 0.0f, size));
    auto line = [&](const Vec3& a, const Vec3& b) {
        Vec2 sa = m_Cam->WorldPointToScreenSpace(a);
        Vec2 sb = m_Cam->WorldPointToScreenSpace(b);
        App::DrawLine(sa.X, sa.Y, sb.X, sb.Y, color.R, color.G, color.B);
    };
    line(c - right, c + right);
    line(c - forward, c + forward);
    line(c, c + Vec3(0.0f, size, 0.0f));
}

const std::vector<SceneEditorScene::ControlGroup>& SceneEditorScene::Controls()
{
    static const std::vector<ControlGroup> groups = {
            {"SCENE VIEW (the editor's camera)",
             {{"W A S D", "Pan along the ground"},
              {"Left / Right", "Orbit around the view's centre"},
              {"Up / Down", "Tilt: look more down / more across"},
              {"E / V", "Move the view up / down"},
              {"Z / C", "Zoom in / out"},
              {"Home", "Reset the view"}}},
            {"OBJECTS",
             {{"Click / drag", "Select / move on the ground"},
              {"R or L / J", "Rotate +15 / -15 degrees"},
              {"I / K", "Raise / lower (Pos Y)"},
              {"F / X", "Duplicate / delete"},
              {"Right click", "Create here, or the object's menu"},
              {"Hierarchy drag", "Parent to another object"}}},
            {"GAME CAMERA (Main Camera object)",
             {{"Select it", "Its row, its cross or its eye marker"},
              {"Move / rotate", "Like any object: its target and heading"},
              {"Inspector", "GameCamera: Distance, Pitch, FOV"},
              {"Right click it", "Align with View / View Through Camera"}}},
            {"LIGHT (Directional Light object)",
             {{"Select it", "Its row, or its sun marker in the air"},
              {"Turn it", "R / J, Rot and Pitch (a sun: only turning)"},
              {"Inspector", "SceneLight: type, colour, shadows"},
              {"Right click", "Aim it, or Aim Light Here on an object"}}},
                        {"EDITING",
             {{"U / Y", "Undo / redo"},
              {"G", "Snap to the grid on / off"},
              {"B", "Show all colliders / selected only"},
              {"P", "Play / stop (through the game camera)"},
              {"Tab", "Software renderer (shadows) / fast triangles"},
              {"H", "Show / hide this panel"},
              {"Esc", "Close a menu or this panel, stop placing"}}},
    };
    return groups;
}

void SceneEditorScene::RenderControlsPanel()
{
    DrawPanel(CONTROLS_X, CONTROLS_BOTTOM, CONTROLS_W, CONTROLS_TOP - CONTROLS_BOTTOM, PANEL_FILL, ACCENT);
    float x = CONTROLS_X + 12.0f;
    float width = CONTROLS_W - 24.0f;
    float y = CONTROLS_TOP - 28.0f;
    Text(x, y, "CONTROLS", ACCENT, width - 80.0f);
    float closeW = std::max(60.0f, UIText::Width("Close") + 16.0f);
    if (Button(NextId(), CONTROLS_X + CONTROLS_W - closeW - 8.0f, CONTROLS_TOP - 30.0f, *m_UI, closeW, WIDGET_H,
               "Close"))
        m_ShowControls = false;
    float keysW = std::min(150.0f, width * 0.36f);
    for (const ControlGroup& group : Controls())
    {
        y -= CONTROLS_ROW + 6.0f;
        if (y < CONTROLS_BOTTOM + 4.0f)
            break;
        Text(x, y, group.Title, ACCENT, width);
        for (const Control& control : group.Controls)
        {
            y -= CONTROLS_ROW;
            if (y < CONTROLS_BOTTOM + 4.0f)
                break;
            Text(x + 8.0f, y, control.Keys, TEXT, keysW - 12.0f);
            Text(x + keysW, y, control.Action, TEXT_DIM, width - keysW);
        }
    }
}

void SceneEditorScene::DrawCameraGizmo(Entity entity, bool selected)
{
    // A game camera: a line from its target up to the eye, and a small
    // pyramid at the eye pointing where it looks (what the game will show)
    SceneCamera::View view = SceneCamera::ViewOf(entity);
    Vec3 eye = SceneCamera::EyeOf(view);
    Vec3 look = view.Target - eye;
    look.Normalize();
    Vec3 right = SceneCamera::Right(view.Yaw);
    Vec3 up = right.Cross(look);
    up.Normalize();
    const Color& color = selected ? ACCENT : CAMERA_COLOR;
    Vec3 forward = m_Cam->Backward * -1.0f;
    // Points behind the editor's view can not be drawn
    auto visible = [&](const Vec3& p) { return (p - m_Cam->Position).Dot(forward) > 0.5f; };
    auto line = [&](const Vec3& a, const Vec3& b) {
        if (!visible(a) || !visible(b))
            return;
        Vec2 sa = m_Cam->WorldPointToScreenSpace(a);
        Vec2 sb = m_Cam->WorldPointToScreenSpace(b);
        App::DrawLine(sa.X, sa.Y, sb.X, sb.Y, color.R, color.G, color.B);
    };
    line(view.Target, eye);
    // The pyramid: its size follows the field of view
    float depth = CAMERA_GIZMO_SIZE;
    float half = depth * std::tan(std::clamp(view.FieldOfView, 20.0f, 150.0f) * 0.5f * 3.14159265f / 180.0f);
    Vec3 center = eye + look * depth;
    Vec3 corners[4] = {center + right * (half * 1.3f) + up * half, center - right * (half * 1.3f) + up * half,
                       center - right * (half * 1.3f) - up * half, center + right * (half * 1.3f) - up * half};
    for (int i = 0; i < 4; ++i)
    {
        line(eye, corners[i]);
        line(corners[i], corners[(i + 1) % 4]);
    }
    // "Up" marker on the top edge
    line(corners[0], center + up * (half * 1.8f));
    line(corners[1], center + up * (half * 1.8f));
    if (visible(eye))
    {
        Vec2 label = m_Cam->WorldPointToScreenSpace(eye);
        Text(label.X + 8.0f, label.Y + 8.0f, m_Editor.NameOf(entity), color, 120.0f);
    }
}

void SceneEditorScene::DrawLightGizmo(Entity entity, bool selected)
{
    // A sun: a ring with rays where the light is, a line to where its centre
    // meets the ground, and parallel rays (directional) or the edges of its
    // cone (spot)
    SceneLighting::Settings settings = SceneLighting::SettingsOf(entity);
    const Color& color = selected ? ACCENT : LIGHT_COLOR;
    Vec3 forward = m_Cam->Backward * -1.0f;
    auto visible = [&](const Vec3& p) { return (p - m_Cam->Position).Dot(forward) > 0.5f; };
    auto line = [&](const Vec3& a, const Vec3& b, const Color& c) {
        if (!visible(a) || !visible(b))
            return;
        Vec2 sa = m_Cam->WorldPointToScreenSpace(a);
        Vec2 sb = m_Cam->WorldPointToScreenSpace(b);
        App::DrawLine(sa.X, sa.Y, sb.X, sb.Y, c.R, c.G, c.B);
    };
    const Vec3 at = settings.Position;
    if (!visible(at))
        return;
    // The ring and its rays face the view
    Vec3 right = m_Cam->CamTransform.GetRight();
    Vec3 up = m_Cam->CamTransform.GetUp();
    constexpr int SEGMENTS = 12;
    constexpr float TWO_PI = 6.2831853f;
    for (int i = 0; i < SEGMENTS; ++i)
    {
        float a0 = TWO_PI * static_cast<float>(i) / SEGMENTS;
        float a1 = TWO_PI * static_cast<float>(i + 1) / SEGMENTS;
        Vec3 p0 = at + (right * std::cos(a0) + up * std::sin(a0)) * (LIGHT_GIZMO_SIZE * 0.5f);
        Vec3 p1 = at + (right * std::cos(a1) + up * std::sin(a1)) * (LIGHT_GIZMO_SIZE * 0.5f);
        line(p0, p1, color);
        if (i % 2 == 0)
            line(at + (p0 - at) * 1.4f, at + (p0 - at) * 2.0f, color);
    }
    // Where it shines
    Vec3 target = SceneLighting::GroundTarget(settings);
    line(at, target, color);
    Vec3 direction = SceneLighting::Direction(settings.Yaw, settings.Light.Pitch);
    Vec3 side = direction.Cross(Vec3(0.0f, 1.0f, 0.0f));
    if (side.Dot(side) < 1e-6f)
        side = Vec3(1.0f, 0.0f, 0.0f);
    side.Normalize();
    Vec3 lift = side.Cross(direction);
    lift.Normalize();
    const Color dim = {color.R * 0.6f, color.G * 0.6f, color.B * 0.6f};
    if (settings.Light.Type == SceneLightType::Directional)
    {
        // A sun: parallel rays (it lights the whole scene the same way)
        for (const Vec3& offset : {side, side * -1.0f, lift, lift * -1.0f})
        {
            Vec3 start = at + offset * (LIGHT_GIZMO_SIZE * 1.5f);
            line(start, start + direction * 4.0f, dim);
        }
        Vec2 label = m_Cam->WorldPointToScreenSpace(at);
        Text(label.X + 12.0f, label.Y + 8.0f, m_Editor.NameOf(entity), color, 140.0f);
        return;
    }
    float spread = std::tan(std::clamp(settings.Light.Spread, 30.0f, 150.0f) * 0.25f * 3.14159265f / 180.0f);
    for (const Vec3& offset : {side, side * -1.0f, lift, lift * -1.0f})
    {
        // Half way out of the cone, to where that ray meets the ground
        Vec3 ray = direction + offset * spread;
        ray.Normalize();
        float length = ray.Y < -1e-3f && at.Y > 0.0f ? at.Y / -ray.Y : 10.0f;
        line(at, at + ray * std::min(length, 200.0f), dim);
    }
    Vec2 label = m_Cam->WorldPointToScreenSpace(at);
    Text(label.X + 12.0f, label.Y + 8.0f, m_Editor.NameOf(entity), color, 140.0f);
}

void SceneEditorScene::DrawColliderGizmo(Entity entity)
{
    PhysicsGizmos::GizmoColor color = PhysicsGizmos::ColorOf(entity, m_Editor.IsPlaying());
    Vec3 forward = m_Cam->Backward * -1.0f;
    for (const PhysicsGizmos::Line& line : PhysicsGizmos::ColliderLines(entity))
    {
        // Points behind the view can not be drawn
        if ((line.A - m_Cam->Position).Dot(forward) <= 0.5f || (line.B - m_Cam->Position).Dot(forward) <= 0.5f)
            continue;
        Vec2 a = m_Cam->WorldPointToScreenSpace(line.A);
        Vec2 b = m_Cam->WorldPointToScreenSpace(line.B);
        App::DrawLine(a.X, a.Y, b.X, b.Y, color.R, color.G, color.B);
    }
}

void SceneEditorScene::DrawColliders()
{
    // Every collider (B / the checkbox), else only the selected object's
    if (m_ShowColliders)
    {
        for (Entity e : ECS.Visit<RigidBody>())
        {
            if (ECS.HasComponent<SceneObject>(e) && !m_Editor.IsField(e))
                DrawColliderGizmo(e);
        }
        return;
    }
    Entity selected = m_Editor.Selected();
    if (!m_Editor.IsPlaying() && selected != NULL_ENTITY && ECS.HasComponent<RigidBody>(selected))
        DrawColliderGizmo(selected);
}

void SceneEditorScene::RenderOverlay()
{
    DrawColliders();
    if (m_Editor.IsPlaying())
        return;
    Entity selected = m_Editor.Selected();
    // Game cameras: where they are and what they see; lights: where they
    // are and where they shine
    for (Entity e : m_Editor.Objects())
    {
        if (m_Editor.IsCamera(e))
            DrawCameraGizmo(e, e == selected);
        else if (m_Editor.IsLight(e))
            DrawLightGizmo(e, e == selected);
    }
    // Empty objects: a cross where they are (they have nothing else to show)
    for (Entity e : m_Editor.Objects())
    {
        if (SceneObjects::IsEmpty(e) && e != selected)
            DrawCross(e, EMPTY_COLOR, CROSS_SIZE);
    }
    if (selected != NULL_ENTITY)
    {
        if (SceneObjects::IsEmpty(selected))
            DrawCross(selected, ACCENT, CROSS_SIZE * 1.4f);
        else
            DrawOutline(selected, ACCENT.R, ACCENT.G, ACCENT.B);
        // Hierarchy links of the selection: to its parent and its children
        auto link = [&](Entity a, Entity b, const Color& c) {
            Vec2 sa = m_Cam->WorldPointToScreenSpace(SceneObjects::GetPosition(a));
            Vec2 sb = m_Cam->WorldPointToScreenSpace(SceneObjects::GetPosition(b));
            App::DrawLine(sa.X, sa.Y, sb.X, sb.Y, c.R, c.G, c.B);
        };
        Entity parent = m_Editor.ParentOf(selected);
        if (parent != NULL_ENTITY)
            link(selected, parent, PARENT_LINK);
        for (Entity child : m_Editor.ChildrenOf(selected))
            link(selected, child, CHILD_LINK);
    }

    if (MouseOverUI() || m_Dragging || m_Menu.IsOpen())
        return;
    Vec3 ground;
    if (!MouseToGround(ground))
        return;
    if (m_PlaceKind != AssetKind::None)
    {
        // Where the asset will go
        Vec3 snapped = m_Editor.ClampToField(SnapPoint(ground));
        Vec2 s = m_Cam->WorldPointToScreenSpace(snapped);
        App::DrawLine(s.X - 8, s.Y, s.X + 8, s.Y, 0.4f, 1.0f, 0.4f);
        App::DrawLine(s.X, s.Y - 8, s.X, s.Y + 8, 0.4f, 1.0f, 0.4f);
        Text(m_UI->mouseX + 14.0f, m_UI->mouseY + 10.0f, m_PlaceName,
             m_PlaceKind == AssetKind::Prefab ? PREFAB_TEXT : TEXT);
        return;
    }
    Entity hovered = PickUnderMouse();
    if (hovered != NULL_ENTITY && hovered != selected && !m_Editor.IsField(hovered))
    {
        if (SceneObjects::IsEmpty(hovered))
            DrawCross(hovered, TEXT, CROSS_SIZE * 1.2f);
        else
            DrawOutline(hovered, 0.8f, 0.8f, 0.8f);
    }
}
