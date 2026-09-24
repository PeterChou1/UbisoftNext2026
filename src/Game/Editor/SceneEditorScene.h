//---------------------------------------------------------------------------------
// SceneEditorScene.h
//---------------------------------------------------------------------------------
//
// In-game scene authoring GUI for Metal Invasion levels, built on the engine's
// immediate mode widgets. All editing logic lives in Editor::SceneEditor
// (headless and unit tested); this class only turns mouse / keyboard input
// into editor calls and draws the interface.
//
//   +----------------------------------------------------------------------+
//   | [scene v] New  Load  Save  Undo  Redo  Play  Menu                     |  toolbar
//   +----------+-----------------------------------------------+-----------+
//   | Palette  |                                               | Inspector |
//   |  Select  |              3D viewport (top down)           |  object   |
//   |  prefabs |       click = place / select, drag = move     |  props    |
//   |  brush   |                                               |  scene    |
//   +----------+-----------------------------------------------+-----------+
//   | status line + key hints                                              |
//   +----------------------------------------------------------------------+
//
// Scenes are saved to data/scenes/<name>.ubsave. "Play" saves a play test copy
// and loads it into the main level through GameManager::RequestLoad.
//
#pragma once

#include "../Scene.h"
#include "app.h"
#include "SceneEditor.h"

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
    static constexpr const char* SCENES_DIRECTORY = "data/scenes";
    static constexpr const char* PLAYTEST_PATH = "saves/editor_playtest.ubsave";

    void Start() override;
    void Setup() override;
    void Update(float deltaTime) override;
    void Render() override;

    // The authored world stays frozen: no physics, particles or AI
    bool SimulatesWorld() const override { return false; }

  private:
    enum class Tool
    {
        Select,
        Place
    };

    // -- Input -------------------------------------------------------------------
    void UpdateCamera(float deltaSeconds);
    void UpdateShortcuts();
    void UpdateViewportMouse();
    bool KeyPressed(App::Key key);
    bool MouseOverUI() const;
    bool MouseToGround(Vec3& groundPoint) const;
    Vec3 SnapPoint(const Vec3& point) const;

    // -- Actions ---------------------------------------------------------------
    void NewScene();
    void SaveScene();
    void LoadScene();
    void PlayScene();
    void DeleteSelected();
    void RotateSelected(float degrees);
    void SelectPrefab(Editor::PrefabType type);
    void RefreshSceneList();
    std::string CurrentSceneName() const;
    std::string CurrentScenePath() const;
    void ResetRenderCaches();
    void SetStatus(const std::string& message, bool error = false);

    // -- Drawing ---------------------------------------------------------------
    // Returns false when a button switched to another scene
    bool RenderToolbar();
    void RenderPalette();
    void RenderInspector();
    void RenderStatusBar();
    void RenderOverlay();
    void DrawWorldMarker(const Vec3& position, float size, float r, float g, float b);
    bool Stepper(int id, float x, float y, float width, const std::string& label, int& delta);

    Editor::SceneEditor m_Editor;
    Tool m_Tool = Tool::Select;
    Editor::PrefabType m_Prefab = Editor::PrefabType::Soldier;
    Editor::PrefabSettings m_Brush;
    bool m_Snap = true;

    // Dragging the selected object
    bool m_Dragging = false;
    bool m_DragRecorded = false;
    Vec3 m_DragOffset;
    Vec3 m_DragStart;

    // Camera orbit around a target on the ground
    Vec3 m_CamTarget = {0, 0, 0};
    float m_CamDistance = 34.0f;

    // Set by Play: the next Setup (coming back from the play test) reopens the
    // played scene instead of starting a new one
    bool m_ResumeFromPlaytest = false;

    std::vector<std::string> m_SceneNames;
    int m_SceneIndex = 0;

    std::string m_Status;
    bool m_StatusIsError = false;
    float m_StatusTimer = 0.0f;

    // Previous frame state of every key, for press (edge) detection
    bool m_KeyHeld[64] = {};

    std::shared_ptr<Camera> m_Cam;
    std::shared_ptr<Lighting> m_Light;
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<UIState> m_UI;
};
