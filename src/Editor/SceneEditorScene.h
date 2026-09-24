//---------------------------------------------------------------------------------
// SceneEditorScene.h
//---------------------------------------------------------------------------------
//
// GUI of the generic scene editor, built on the engine's immediate mode
// widgets. All editing logic lives in Editor::SceneEditor; this class turns
// mouse / keyboard input into editor calls and draws the interface.
//
//   +-------------------------------------------------------------------------+
//   | [scene v] New Load Save Undo Redo  Play  Scene/Object   Editing x *     |
//   +----------+-------------------------------------------------+------------+
//   | PALETTE  |                                                 | INSPECTOR  |
//   |  Select  |          field seen by the 3D renderer          | object or  |
//   |  shapes  |     click = place / select, drag = move         | scene      |
//   |  BRUSH   |                                                 | settings   |
//   +----------+-------------------------------------------------+------------+
//   | status line + key hints                                                 |
//   +-------------------------------------------------------------------------+
//
// Play runs the scene's C++ scripts inside the editor (physics, scripts,
// particles on). Stop restores the scene exactly as it was before Play.
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
    bool MouseOverUI() const;
    bool MouseToGround(Vec3& groundPoint) const;
    Vec3 SnapPoint(const Vec3& point) const;

    // -- Actions -------------------------------------------------------------------
    void NewScene();
    void SaveScene();
    void LoadScene();
    void TogglePlay();
    void DeleteSelected();
    void DuplicateSelected();
    void RotateSelected(float degrees);
    void SelectKind(Editor::ObjectKind kind);
    void RefreshSceneList();
    std::string CurrentSceneName() const;
    std::string CurrentScenePath() const;
    void SetStatus(const std::string& message, bool error = false);

    // -- Drawing -------------------------------------------------------------------
    bool RenderToolbar();
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
    Vec3 m_DragOffset;

    Vec3 m_CamTarget = {0, 0, 0};
    float m_CamDistance = 30.0f;

    std::vector<std::string> m_SceneNames;
    int m_SceneIndex = 0;
    std::vector<std::string> m_Models;

    std::string m_Status;
    bool m_StatusIsError = false;
    float m_StatusTimer = 0.0f;

    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<UIState> m_UI;
};
