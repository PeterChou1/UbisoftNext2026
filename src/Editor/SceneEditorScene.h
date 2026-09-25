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
//   | PALETTE  |                                                 | INSPECTOR  |
//   |  Select  |          field seen by the 3D renderer          | typed and  |
//   |  shapes  |  click = place / select, drag = move, drag a    | stepped    |
//   |  BRUSH   |  palette shape onto the field to drop it there  | values     |
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
// A tutorial for the basic actions is in docs/EditorTutorial.md.
//
#pragma once

#include "Scene.h"
#include "SceneEditor.h"
#include "app.h"

#include <memory>
#include <string>
#include <vector>

class Camera;
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
    void SetStatus(const std::string& message, bool error = false);

    // -- Drawing -------------------------------------------------------------------
    bool RenderToolbar();
    void RenderSceneList();
    void RenderPalette();
    void RenderInspector();
    void RenderObjectInspector(float x, float& y);
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

    Vec3 m_CamTarget = {0, 0, 0};
    float m_CamDistance = 30.0f;

    std::string m_SceneDirectory;
    std::vector<std::string> m_SceneNames;
    int m_SceneIndex = 0;
    std::string m_DocName;
    // Scene picked while the open one had unsaved changes (-1 = none)
    int m_PendingSceneIndex = -1;
    std::vector<std::string> m_Models;

    std::string m_Status;
    bool m_StatusIsError = false;
    float m_StatusTimer = 0.0f;

    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<UIState> m_UI;
};
