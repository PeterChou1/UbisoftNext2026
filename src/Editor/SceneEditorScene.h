//---------------------------------------------------------------------------------
// SceneEditorScene.h
//---------------------------------------------------------------------------------
//
// GUI of the generic scene editor, built on the engine's immediate mode
// widgets. All editing logic lives in Editor::SceneEditor; this class turns
// mouse / keyboard input into editor calls and draws the interface.
//
//   +-------------------------------------------------------------------------+
//   | [scene v] New Revert Save Undo Redo  Play  Scene   Name [my_level] *     |
//   +----------+-------------------------------------------------+------------+
//   | PALETTE /|                                                 | INSPECTOR  |
//   | HIERARCHY|          field seen by the 3D renderer          | typed and  |
//   |  shapes  |  click = place / select, drag = move, drag a    | stepped    |
//   |  BRUSH   |  palette shape onto the field to drop it there  | values,    |
//   |          |                                                 | components |
//   +----------+-------------------------------------------------+------------+
//   | status line + key hints                                                 |
//   +-------------------------------------------------------------------------+
//
// Scenes are files in the scene directory (data/scenes): the list shows every
// file, picking one opens it, New creates and saves a new one, the name field
// renames the open scene (and its file).
//
// Play runs the scene's C++ scripts inside the editor (physics, scripts,
// particles on). Stop restores the scene exactly as it was before Play.
//
// The left panel has two tabs: the Palette (what to place, the brush) and the
// Hierarchy, the scene's objects as a tree of Transforms (a child under its
// parent). Drag a row onto another to make it a child, onto SCENE to make it a
// top level object. Empty objects (just a Transform) are drawn as a cross.
//
// The object inspector has two tabs: Properties (shape, body, tag, script) and
// Components, where components are added / removed and every reflected field
// (Reflection/Reflection.h) gets a widget generated from its description.
//
// Tutorials: docs/EditorTutorial.md (basic actions) and
// docs/ComponentsTutorial.md (writing components).
//
#pragma once

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
     * \brief Import a model (what the palette's Import box does)
     */
    /**
     * \brief Show the Components tab (true) or the Properties tab
     */
    void ShowComponents(bool show) { m_ShowComponents = show; }

    /**
     * \brief Show the Hierarchy tab (true) or the Palette in the left panel
     */
    void ShowHierarchy(bool show) { m_ShowHierarchy = show; }

    /**
     * \brief Rows of the hierarchy tree as drawn: object and depth (collapsed
     *        branches are left out)
     */
    std::vector<std::pair<Entity, int>> HierarchyRows() const;

    /**
     * \brief Create an empty object under the selected one (or at the view's
     *        centre when nothing is selected), what "New Empty" does
     */
    Entity AddEmpty();

    bool Import(const std::string& path)
    {
        m_ImportPath = path;
        ImportModel();
        return !m_StatusIsError;
    }
    const Editor::PlaceSettings& Brush() const { return m_Brush; }

    // Where the editor looks for .obj files typed by name
    static constexpr const char* IMPORT_DIRECTORY = "data/import";

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

  private:
    enum class Tool
    {
        Select,
        Place
    };

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

    // -- Actions -------------------------------------------------------------------
    void RevertScene();
    void TogglePlay();
    void DeleteSelected();
    void DuplicateSelected();
    void RotateSelected(float degrees);
    void SelectKind(Editor::ObjectKind kind);
    void RefreshSceneList();
    std::string UniqueSceneName() const;
    /**
     * \brief Import the .obj named in the palette's import box (a path, or a
     *        file name in data/import/)
     */
    void ImportModel();
    void SetStatus(const std::string& message, bool error = false);

    // -- Drawing -------------------------------------------------------------------
    bool RenderToolbar();
    void RenderSceneList();
    void RenderLeftPanel();
    void RenderPalette(float x, float y);
    void RenderHierarchy(float x, float y);
    void DrawCross(Entity entity, const Color& color, float size);
    void RenderInspector();
    void RenderObjectInspector(float x, float& y);
    void RenderObjectProperties(Entity e, float x, float& y);
    void RenderComponents(Entity e, float x, float& y);
    /**
     * \brief Widget(s) of one reflected field, generated from its FieldInfo.
     *        False when there was no room left to draw it
     */
    bool RenderField(Entity e, const std::string& component, const Reflection::FieldInfo& field, float x, float& y);
    void RenderSceneInspector(float x, float& y);
    void RenderStatusBar();
    void RenderOverlay();
    void DrawOutline(Entity entity, float r, float g, float b);
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
    int NextId() { return m_NextId++; }
    // Fixed ids of the Components tab's text fields, in drawing order
    int NextFieldId();

    Editor::SceneEditor m_Editor;
    Tool m_Tool = Tool::Select;
    Editor::ObjectKind m_Kind = Editor::ObjectKind::Rectangle;
    Editor::PlaceSettings m_Brush;
    bool m_Snap = true;
    bool m_ShowScene = false;
    int m_NextId = 0;

    bool m_Dragging = false;
    bool m_DragRecorded = false;
    bool m_DragMoved = false;
    float m_DragHeight = 0.0f;
    Vec3 m_DragOffset;
    // A palette shape is being dragged onto the field
    bool m_PaletteDrag = false;
    // Object whose values the inspector's text fields show
    Entity m_FieldsEntity = NULL_ENTITY;
    // Object inspector tab: properties or components
    bool m_ShowComponents = false;
    // Components whose fields are folded away
    std::set<std::string> m_Folded;
    // Component picked in the "Add" picker
    int m_AddIndex = 0;
    int m_NextFieldId = 0;
    // Tooltip of the field under the mouse (status bar)
    std::string m_Hint;

    // -- Hierarchy tab -------------------------------------------------------------
    bool m_ShowHierarchy = false;
    // Objects whose children are hidden in the tree
    std::set<Entity> m_Collapsed;
    // First row shown (the tree scrolls when it is longer than the panel)
    int m_TreeScroll = 0;
    // Row pressed in the tree, and whether it is being dragged onto another
    Entity m_TreePressed = NULL_ENTITY;
    bool m_TreeDragging = false;
    float m_TreePressX = 0.0f;
    float m_TreePressY = 0.0f;
    // Selection the tree last showed (to reveal a newly selected object)
    Entity m_TreeSelected = NULL_ENTITY;

    Vec3 m_CamTarget = {0, 0, 0};
    float m_CamDistance = 30.0f;

    std::string m_SceneDirectory;
    std::vector<std::string> m_SceneNames;
    int m_SceneIndex = 0;
    std::string m_DocName;
    // Scene picked while the open one had unsaved changes (-1 = none)
    int m_PendingSceneIndex = -1;
    std::vector<std::string> m_Models;
    std::string m_ImportPath;

    std::string m_Status;
    bool m_StatusIsError = false;
    float m_StatusTimer = 0.0f;

    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<UIState> m_UI;
};
