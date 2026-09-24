#include "SampleScenes.h"

#include "BlackBoard.h"
#include "ECSManager.h"
#include "GameState.h"
#include "UIState.h"

extern ECSManager ECS;

using Editor::PrefabSettings;
using Editor::PrefabType;
using Editor::SceneEditor;

namespace
{
    // Place a rows x cols block of units centred on (x, z) like the game's
    // Create*Battalion functions (0.5 spacing)
    void PlaceBlock(SceneEditor& editor,
                    PrefabType type,
                    float x,
                    float z,
                    int rows,
                    int cols,
                    const PrefabSettings& settings)
    {
        const float spacing = 0.5f;
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                float offsetX = (col - (cols - 1) * 0.5f) * spacing;
                float offsetZ = (row - (rows - 1) * 0.5f) * spacing;
                editor.Place(type, {x + offsetX, 0, z + offsetZ}, settings);
            }
        }
    }

    PrefabSettings Unit(int health, int battalion)
    {
        PrefabSettings s;
        s.Health = health;
        s.Battalion = battalion;
        return s;
    }

    PrefabSettings Enemy(int health, float speed)
    {
        PrefabSettings s;
        s.Health = health;
        s.EnemySpeed = speed;
        return s;
    }

    PrefabSettings CrystalOf(int amount)
    {
        PrefabSettings s;
        s.CrystalAmount = amount;
        return s;
    }

    PrefabSettings Rotated(float degrees)
    {
        PrefabSettings s;
        s.YawDegrees = degrees;
        return s;
    }

    //-----------------------------------------------------------------------------

    void EmptyArena(SceneEditor& editor)
    {
        editor.NewScene();
    }

    void FirstContact(SceneEditor& editor)
    {
        editor.NewScene();
        PlaceBlock(editor, PrefabType::Soldier, 4.0f, -4.0f, 3, 3, Unit(100, 1));
        PlaceBlock(editor, PrefabType::Support, -4.0f, -3.0f, 1, 2, Unit(60, 2));
        editor.Place(PrefabType::Crystal, {-9.0f, 0, -6.0f}, CrystalOf(30));
        editor.Place(PrefabType::Crystal, {-12.0f, 0, 2.0f}, CrystalOf(30));
        editor.Place(PrefabType::Crystal, {10.0f, 0, -11.0f}, CrystalOf(20));
        PlaceBlock(editor, PrefabType::EnemySoldier, 16.0f, 15.0f, 2, 3, Enemy(100, 0.0012f));
        editor.SetStartingCrystals(25);
    }

    void Fortress(SceneEditor& editor)
    {
        editor.NewScene();
        // Wall ring around the base with an opening to the south
        editor.Place(PrefabType::Wall, {0.0f, 0, 6.0f}, Rotated(90.0f));
        editor.Place(PrefabType::Wall, {6.0f, 0, 0.0f});
        editor.Place(PrefabType::Wall, {-6.0f, 0, 0.0f});
        editor.Place(PrefabType::Wall, {-4.0f, 0, -6.0f}, Rotated(90.0f));
        Entity tank = editor.Place(PrefabType::PlayerTank, {0.0f, 0, -8.0f}, Unit(200, 3));
        editor.SetYaw(tank, 180.0f);
        PlaceBlock(editor, PrefabType::Soldier, 3.0f, -3.0f, 2, 2, Unit(100, 1));
        PlaceBlock(editor, PrefabType::Support, -3.0f, 3.0f, 1, 3, Unit(60, 2));
        editor.Place(PrefabType::Crystal, {-2.0f, 0, 3.5f}, CrystalOf(45));
        editor.Place(PrefabType::Crystal, {2.5f, 0, 3.5f}, CrystalOf(45));
        editor.SetStartingCrystals(60);
        editor.SetSpawnVolume(4);
    }

    void TankBattle(SceneEditor& editor)
    {
        editor.NewScene();
        Entity left = editor.Place(PrefabType::PlayerTank, {-5.0f, 0, -5.0f}, Unit(200, 3));
        Entity right = editor.Place(PrefabType::PlayerTank, {5.0f, 0, -5.0f}, Unit(250, 4));
        editor.SetYaw(left, 45.0f);
        editor.SetYaw(right, 315.0f);
        editor.Place(PrefabType::EnemyTank, {-12.0f, 0, 14.0f}, Unit(200, 0));
        editor.Place(PrefabType::EnemyTank, {0.0f, 0, 18.0f}, Unit(300, 0));
        editor.Place(PrefabType::EnemyTank, {12.0f, 0, 14.0f}, Unit(200, 0));
        PlaceBlock(editor, PrefabType::EnemySoldier, 0.0f, 12.0f, 2, 4, Enemy(120, 0.0015f));
        editor.SetRoundNumber(3);
        editor.SetSpawnVolume(5);
        editor.SetStartingCrystals(40);
    }

    void CrystalRush(SceneEditor& editor)
    {
        editor.NewScene();
        const float positions[][2] = {
                {-15, -15}, {-10, 12}, {14, -9}, {18, 6}, {-18, 3}, {7, 17}, {-3, -18}};
        int amount = 10;
        for (const auto& p : positions)
        {
            editor.Place(PrefabType::Crystal, {p[0], 0, p[1]}, CrystalOf(amount));
            amount += 10;
        }
        PlaceBlock(editor, PrefabType::Support, -6.0f, -6.0f, 2, 3, Unit(60, 2));

        // Exercise the editing workflow: place, change, move, undo, redo
        Entity scout = editor.Place(PrefabType::Soldier, {3.0f, 0, 3.0f}, Unit(100, 1));
        editor.SetHealth(scout, 150);
        editor.Move(scout, {5.5f, 0, 2.0f});
        Entity mistake = editor.Place(PrefabType::EnemyTank, {1.0f, 0, 1.0f});
        (void)mistake;
        editor.Undo(); // remove the misplaced enemy tank
        editor.Undo(); // move the scout back
        editor.Redo(); // ... and to its final spot again

        Entity fast = editor.Place(PrefabType::EnemySoldier, {20.0f, 0, 20.0f}, Enemy(80, 0.001f));
        editor.SetEnemySpeed(fast, 0.0025f);
        editor.SetStartingCrystals(10);
    }
} // namespace

namespace SampleScenes
{
    const std::vector<SampleScene>& All()
    {
        static const std::vector<SampleScene> scenes = {
                {"empty_arena", "Minimal playable scene: ground, base and selector", EmptyArena},
                {"first_contact",
                 "Soldier battalion, miners, crystals and a raiding party",
                 FirstContact},
                {"fortress",
                 "Base walled in (both wall orientations) with a tank on guard",
                 Fortress},
                {"tank_battle", "Player tanks against enemy tanks in round 3", TankBattle},
                {"crystal_rush",
                 "Many deposits, authored with undo/redo and property edits",
                 CrystalRush},
        };
        return scenes;
    }

    void EnsureEditorResources()
    {
        if (!ECS.HasResource<GameState>())
            ECS.RegisterResource(GameState());
        if (!ECS.HasResource<BlackBoard>())
            ECS.RegisterResource(BlackBoard());
        if (!ECS.HasResource<UIState>())
            ECS.RegisterResource(UIState());
    }
} // namespace SampleScenes
