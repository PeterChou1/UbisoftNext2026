//---------------------------------------------------------------------------------
// MIGame.h
//---------------------------------------------------------------------------------
//
// MetalInvasion: the scene script that drives the whole game, replacing the
// original MainLevel scene and its systems (GameRoundControllerSystem,
// EnemyControllerSystem, UnitControllerSystem, BuildObstaclesSystem,
// GameCameraController, MainLevelUI) and the GameState / BlackBoard resources.
//
// Rules (same as the original):
//   Spawn        crystal deposits appear around the base (up to CrystalCount)
//   Preparation  PrepTime seconds to buy units / walls and mine (Enter skips)
//   Invasion     enemy battalions and tanks arrive every SpawnInterval seconds
//                for InvasionTime seconds; once they are all dead the next
//                round starts, with more enemies and a longer invasion
//   Game over    when the base is destroyed
//
// Controls: WASD pan the camera. Left click a unit selects its battalion,
// left click the ground sends the selection there, right click deselects,
// Space + click merges battalions. Click the base to open the shop.
//
#pragma once

#include "MINavigation.h"
#include "MIPrefabs.h"
#include "Scripting/Script.h"

#include <random>
#include <vector>

class MetalInvasion : public SceneScript
{
  public:
    enum class Phase
    {
        Spawn,
        Preparation,
        Invasion,
        GameOver
    };

    enum class Mode
    {
        Command,   // select units, give move orders
        BaseMenu,  // the shop is open
        PlaceWall  // a wall follows the mouse
    };

    enum class Item
    {
        Soldiers,
        Support,
        Tank,
        Wall
    };

    static constexpr int SOLDIERS_COST = 10;
    static constexpr int SUPPORT_COST = 15;
    static constexpr int TANK_COST = 50;
    static constexpr int WALL_COST = 10;

    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;
    void OnRender() override;

    // -- State ----------------------------------------------------------------------
    Phase GetPhase() const { return m_Phase; }
    Mode GetMode() const { return m_Mode; }
    int Round() const { return m_Round; }
    int Crystals() const { return m_Crystals; }
    int SpawnVolume() const { return m_SpawnVolume; }
    float PhaseTimeLeft() const;
    Entity BaseEntity() const { return m_Base; }
    float BaseHealth();

    // -- Commands (used by the mouse / keyboard handling and by the tests) ----------
    /**
     * \brief Buy an item from the base menu. Units appear in front of the
     *        base, a wall enters wall placement. False if too expensive
     */
    bool Purchase(Item item);
    void OpenBaseMenu();
    void CloseBaseMenu();
    // Wall placement: move the preview, try to build it
    void MoveWallPreview(const Vec3& position);
    void RotateWallPreview();
    bool PlaceWall();
    void CancelWall();
    bool WallPreviewValid();
    Entity WallPreview() const { return m_WallPreview; }

    void Select(Entity unit, bool merge);
    void ClearSelection();
    std::vector<Entity> SelectedUnits();
    void OrderMove(const Vec3& target);
    void SkipPreparation();

    // -- Used by the object scripts -----------------------------------------------
    MI::FlowField& UnitField() { return m_UnitField; }
    MI::FlowField& EnemyField() { return m_EnemyField; }
    bool HasMoveOrder() const { return m_HasMoveOrder; }
    void AddCrystals(int amount) { m_Crystals += amount; }
    void OnBaseDestroyed();
    // A crystal / wall disappeared: free its cells on the path finding grids
    void OnObstacleRemoved(Entity entity);

  private:
    void UpdateCamera(float deltaSeconds);
    void UpdateRound(float deltaSeconds);
    void HandleMouse();
    void SpawnCrystals();
    void SpawnEnemyWave();
    void AddObstacle(Entity entity);
    Entity UnitUnderMouse(const Vec3& ground);
    float Random01();

    void RenderHud();
    void RenderBaseMenu();
    void RenderGameOver();

    Phase m_Phase = Phase::Spawn;
    Mode m_Mode = Mode::Command;
    int m_Round = 1;
    int m_Crystals = 25;
    int m_SpawnVolume = 1;
    float m_PrepLeft = 0.0f;
    float m_InvasionLeft = 0.0f;
    float m_InvasionTime = 20.0f;
    float m_SpawnTimer = 0.0f;
    int m_NextBattalion = 0;

    Entity m_Base = NULL_ENTITY;
    bool m_BaseHovered = false;
    bool m_MenuJustOpened = false;

    Entity m_WallPreview = NULL_ENTITY;
    bool m_WallRotated = false;
    bool m_WallPreviewShownValid = true;

    bool m_HasMoveOrder = false;
    MI::FlowField m_UnitField;
    MI::FlowField m_EnemyField;

    Vec3 m_CamTarget = {0, 0, 0};
    float m_CamDistance = 16.0f;
    std::mt19937 m_Random;
};
