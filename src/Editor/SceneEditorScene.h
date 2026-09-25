//---------------------------------------------------------------------------------
// SceneEditorScene.h
//---------------------------------------------------------------------------------
//
// GUI of the generic scene editor, in the style of Unity's editor, built on
// the engine's immediate mode widgets. All editing logic lives in
// Editor::SceneEditor; this class turns mouse / keyboard input into editor
// calls and draws the interface (layout in EditorStyle.h):
//
//   - Toolbar: scene list, New / Revert / Save / Undo / Redo, Play, the
//     Scene settings toggle and the scene's name. In the prefab editor: the
//     prefab's name, Save Prefab, Undo / Redo and Back to Scene.
//   - Hierarchy (left, top): the scene's objects as a tree of Transforms.
//     Drag rows to parent them; "+" and right clicks create objects.
//   - Assets (left, bottom): prefabs and models. Click one, then click the
//     field to place it (or drag it onto the field). New Prefab, Import .obj.
//   - Scene view (middle): the 3D renderer. Click / drag objects; right
//     click opens the context menu (create here, or the object's actions).
//   - Inspector (right): the selected object as one scrollable list of
//     component sections (Transform, Shape2D or Mesh, RigidBody, Script, the
//     project's components, Prefab), with Add Component at the bottom.
//
// Prefab editor: "Edit Prefab" (on an instance or an asset) or "New Prefab"
// puts the scene aside and opens the prefab alone on an empty stage. Save
// Prefab writes data/prefabs/<name>.ubprefab; Back to Scene brings the scene
// back and updates its instances of the prefab.
//
// Play runs the scene's C++ scripts inside the editor. Stop restores the
// scene exactly as it was before Play.
//
// Tutorials: docs/EditorTutorial.md (the editor), docs/ComponentsTutorial.md
// (writing components).
//
#pragma once

#include "ContextMenu.h"
#include "Scene.h"
#include "SceneEditor.h"
#include "app.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

class Camera;
struct Color;
class Lighting;
class GameOptions;
class UIState;

class SceneEditorScene : public Scene
{
  public:
    void Start() override;
    void Setup() override;
    void Update(float deltaTime) override;
    void Render() override;

    // The world only simulates (physics, scripts) while playing
    bool SimulatesWorld() const override { return m_Editor.IsPlaying(); }

    Editor::SceneEditor& GetEditor() { return m_Editor; }

    /**
     * \brief Folder holding the scene files (default data/scenes). Takes
     *        effect on the next Setup
     */
    void SetSceneDirectory(const std::string& directory) { m_SceneDirectory = directory; }

    /**
     * \brief Folder holding the prefab files (default data/prefabs)
     */
    void SetPrefabDirectory(const std::string& directory);
    const std::string& PrefabDirectory() const { return m_PrefabDirectory; }

    // Where the editor looks for .obj files typed by name
    static constexpr const char* IMPORT_DIRECTORY = "data/import";

    /**
     * \brief Import a model (what the Assets' Import box does). The model is
     *        then ready to place
     */
    bool Import(const std::string& path)
    {
        m_ImportPath = path;
        ImportModel();
        return !m_StatusIsError;
    }

    // -- Scene documents (also used by the tests) ---------------------------------------
    const std::string& SceneName() const { return m_DocName; }
    const std::vector<std::string>& SceneNames() const { return m_SceneNames; }
    std::string ScenePath(const std::string& name) const;
    /**
     * \brief Create a new empty scene named scene_<n>, save it and open it
     */
    void NewDocument();
    bool OpenScene(const std::string& name);
    bool SaveDocument();
    /**
     * \brief Rename the open scene and its file
     */
    bool RenameDocument(const std::string& newName);

    // -- Prefabs --------------------------------------------------------------------------
    /**
     * \brief Save an object and its children as a new prefab (named after the
     *        object) and make the object an instance of it
     */
    bool SaveAsPrefab(Entity root);
    /**
     * \brief Open the prefab editor on a prefab file / on a new prefab
     */
    bool EditPrefab(const std::string& name);
    bool NewPrefab();
    bool SavePrefab();
    /**
     * \brief Leave the prefab editor. With unsaved changes the first call
     *        only warns (false); a second call discards them
     */
    bool BackToScene();
    bool InPrefabMode() const { return m_PrefabMode; }
    const std::string& PrefabName() const { return m_PrefabName; }
    const std::vector<std::string>& PrefabNames() const { return m_Prefabs; }

    /**
     * \brief Start placing a prefab / model: the next click on the field
     *        places it ("" stops)
     */
    void StartPlacingPrefab(const std::string& name);
    void StartPlacingModel(const std::string& name);
    const std::string& PlacingAsset() const { return m_PlaceName; }

    // -- Context menus (also used by the tests) ------------------------------------------
    ContextMenu& Menu() { return m_Menu; }
    /**
     * \brief Items of the "create" menu: new objects at `position`, children
     *        of `parent` when given
     */
    std::vector<MenuItem> CreateItems(const Vec3& position, Entity parent);
    /**
     * \brief Items of an object's menu
     */
    std::vector<MenuItem> ObjectItems(Entity entity);

    // -- Hierarchy ----------------------------------------------------------------------
    /**
     * \brief Rows of the hierarchy tree as drawn: object and depth (collapsed
     *        branches are left out)
     */
    std::vector<std::pair<Entity, int>> HierarchyRows() const;
    /**
     * \brief Inspector scroll offset (0 = top) and the height of its content
     */
    float InspectorScroll() const { return m_InspectorScroll; }
    float InspectorContentHeight() const { return m_InspectorContent; }

  private:
    // -- Input ---------------------------------------------------------------------
    void UpdateCamera(float deltaSeconds);
    void UpdateShortcuts();
    void UpdateViewportMouse();
    void BeginDrag(Entity entity, float height);
    bool MouseOverUI() const;
    bool MouseAtHeight(float height, Vec3& point) const;
    bool MouseToGround(Vec3& groundPoint) const { return MouseAtHeight(0.0f, groundPoint); }
    Entity PickUnderMouse(float* hitHeight = nullptr) const;
    Vec3 SnapPoint(const Vec3& point) const;
    Vec3 ViewCenter() const;
    void OpenViewportMenu();

    // -- Actions -------------------------------------------------------------------
    void RevertScene();
    void TogglePlay();
    void DeleteSelected();
    void DuplicateSelected();
    void RotateSelected(float degrees);
    void CreateObject(Editor::ObjectKind kind, const Vec3& position, Entity parent, const std::string& model = "");
    void PlaceAsset(const Vec3& position);
    void RefreshSceneList();
    void RefreshAssets();
    std::string UniqueSceneName() const;
    std::string UniquePrefabName(const std::string& base) const;
    /**
     * \brief Import the .obj named in the import box (a path, or a file name
     *        in data/import/)
     */
    void ImportModel();
    void SetStatus(const std::string& message, bool error = false);

    // -- Drawing -------------------------------------------------------------------
    bool RenderToolbar();
    void RenderSceneList();
    void RenderStatusBar();
    void RenderOverlay();
    void DrawOutline(Entity entity, float r, float g, float b);
    void DrawCross(Entity entity, const Color& color, float size);

    // Left panel (EditorLeftPanel.cpp)
    void RenderLeftPanel();
    void RenderHierarchy(float top, float bottom);
    void RenderAssets(float top, float bottom);

    // Inspector (EditorInspector.cpp)
    void RenderInspector();
    void RenderObjectInspector(Entity e, float x, float width);
    void RenderSceneInspector(float x, float width);
    void RenderPlayingInspector(float x, float width);
    /**
     * \brief Next row of the scrolling inspector: `y` gets its bottom. True
     *        when the row is fully visible (draw it); hidden rows only take
     *        their place
     */
    bool Row(float height, float& y);
    /**
     * \brief Foldable section header with an optional Remove button. True
     *        when the section is open. `removed` is set when Remove was clicked
     */
    bool Section(const std::string& title, const std::string& summary, bool removable, float x, float width, bool& removed);
    /**
     * \brief Widget(s) of one reflected field, generated from its FieldInfo
     */
    void RenderField(Entity e, const std::string& component, const Reflection::FieldInfo& field, float x, float width);

    // Widgets
    int Stepper(float x,
                float y,
                float width,
                const std::string& label,
                const char* minus = "-",
                const char* plus = "+");
    /**
     * \brief "Label [typed value] [-][+]": true when the value was typed or
     *        stepped (value updated)
     */
    bool NumberRow(float x,
                   float y,
                   float width,
                   const std::string& label,
                   int fieldId,
                   float& value,
                   float step,
                   const char* format = "%.2f");
    /**
     * \brief Vertical scrollbar on [x, bottom .. top]. `scroll` goes from 0
     *        (top) to content - view. Drag the thumb or click the track
     */
    void Scrollbar(int id, float x, float bottom, float top, float content, float& scroll);
    int NextId() { return m_NextId++; }
    // Fixed ids of reflected fields' text boxes, in drawing order
    int NextFieldId();
    // A text field with this id was drawn this frame
    void FieldDrawn(int id) { m_DrawnFields.insert(id); }

    Editor::SceneEditor m_Editor;
    ContextMenu m_Menu;
    bool m_ShowScene = false;
    bool m_Snap = true;
    int m_NextId = 0;

    bool m_Dragging = false;
    bool m_DragRecorded = false;
    bool m_DragMoved = false;
    float m_DragHeight = 0.0f;
    Vec3 m_DragOffset;
    // Object whose values the inspector's text fields show
    Entity m_FieldsEntity = NULL_ENTITY;

    // -- Placing assets (prefabs / models) -----------------------------------------------
    enum class AssetKind
    {
        None,
        Prefab,
        Model
    };
    AssetKind m_PlaceKind = AssetKind::None;
    std::string m_PlaceName;
    // An asset row is being dragged onto the scene view
    bool m_AssetDrag = false;

    // -- Inspector ------------------------------------------------------------------------
    // Sections folded away (by component name)
    std::set<std::string> m_Folded;
    // Component picked in the Add Component picker
    int m_AddIndex = 0;
    int m_NextFieldId = 0;
    std::set<int> m_DrawnFields;
    // Tooltip of the field under the mouse (status bar)
    std::string m_Hint;
    float m_InspectorScroll = 0.0f;
    float m_InspectorContent = 0.0f;
    // Row layout of the inspector being drawn
    float m_RowCursor = 0.0f;
    float m_ViewTop = 0.0f;
    float m_ViewBottom = 0.0f;

    // -- Scrollbars -----------------------------------------------------------------------
    int m_ScrollDragId = 0;
    float m_ScrollGrab = 0.0f;

    // -- Hierarchy ------------------------------------------------------------------------
    std::set<Entity> m_Collapsed;
    float m_TreeScroll = 0.0f;
    Entity m_TreePressed = NULL_ENTITY;
    bool m_TreeDragging = false;
    float m_TreePressX = 0.0f;
    float m_TreePressY = 0.0f;
    // Selection the tree last showed (to reveal a newly selected object)
    Entity m_TreeSelected = NULL_ENTITY;

    // -- Assets ---------------------------------------------------------------------------
    std::vector<std::string> m_Prefabs;
    std::vector<std::string> m_Models;
    std::string m_ImportPath;
    float m_AssetScroll = 0.0f;

    // -- Camera ---------------------------------------------------------------------------
    Vec3 m_CamTarget = {0, 0, 0};
    float m_CamDistance = 30.0f;

    // -- Documents ------------------------------------------------------------------------
    std::string m_SceneDirectory;
    std::string m_PrefabDirectory = Prefab::DIRECTORY;
    std::vector<std::string> m_SceneNames;
    int m_SceneIndex = 0;
    std::string m_DocName;
    // Scene picked while the open one had unsaved changes (-1 = none)
    int m_PendingSceneIndex = -1;

    // -- Prefab editor --------------------------------------------------------------------
    bool m_PrefabMode = false;
    std::string m_PrefabName;
    Editor::SceneEditor::Session m_Session;
    bool m_PrefabSaved = false;
    bool m_ExitPending = false;
    // The scene's camera while the prefab stage is shown
    Vec3 m_SceneCamTarget = {0, 0, 0};
    float m_SceneCamDistance = 30.0f;

    std::string m_Status;
    bool m_StatusIsError = false;
    float m_StatusTimer = 0.0f;

    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<UIState> m_UI;
};
