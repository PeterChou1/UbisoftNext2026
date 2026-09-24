# 03 – Game integration (Metal Invasion)

## What is saved

| Kind | Types |
|---|---|
| Engine components | `Transform` (hierarchy, TRS, affine + inverse, slice plane, dirty flag), `RigidBody` (full physics state including private mass/inertia/restitution, shape, AABB), `Mesh` (asset id), `FragShaderTag` / `VertShaderTag` (shader type), `Particle`, `Emitter`, `UITarget`, `AIObstacle` |
| Game components | `PlayerControlUnit`, `BasicEnemyUnit`, `CrystalDeposit`, `PlayerBaseComponent` (saved as `"PlayerBase"`), `TankBullet`, `Explosion`, `LaserProjectile` |
| `GameState` resource | Round number and phase, prep/invasion timers, crystal inventory, spawn volume/interval, difficulty scaling, obstacle being placed, camera lerp state |
| `BlackBoard` resource | Unit/enemy targets, tank target maps, last-known locations, line-of-sight, patrol targets, delta time, vector field **configuration** |
| `UIState` resource | **Only** the interaction mode (`state`) and the wall orientation (`flipped`) |

## What is rebuilt after loading (not saved)

| State | Why it is not saved | How it is rebuilt |
|---|---|---|
| Behaviour trees (`BlackBoard::BehaviorTreeDataBase` and the `BehaviorTree` tag component) | Node graphs hold code (virtual `Process`) and references into the blackboard | `RestoreMainLevelRuntimeState()` reattaches the right tree to each unit |
| Vector field grids | Derived data, recomputed from the obstacles | Grid recreated from the saved configuration, then `SetMapObstacles(all AIObstacle)` |
| Render caches, shader instances | Only valid for the current session | Handles reset on load, then `MeshHandler`/`ShaderHandler` rebuild them |
| Camera, Lighting, GameOptions, buffers, UIState mouse/widget fields | Scene configuration or per-frame input/scratch data | Recreated by the normal `Scene::Setup()` / next input poll |

In-progress state inside AI nodes, such as a unit's weapon reload countdown, is
not saved. After a load each unit starts with a fresh reload timer.

## Refactor: behaviour-tree setup split from entity creation

To reattach AI to loaded units, the tree-building part of each `Create*`
function was moved into its own function. The `Create*` functions now call these
new functions, so newly spawned units behave exactly as before.

| File | New function | Called by |
|---|---|---|
| `PlayerUnits.cpp/.h` | `AttachSoldierBehaviour(Entity)` | `CreateSoldierUnits` |
| | `AttachSupportBehaviour(Entity)` | `CreateSupportUnits` |
| | `AttachPlayerTankBehaviour(Entity tank, Entity cannon)` | `CreateTank` |
| `BasicEnemyUnit.cpp/.h` | `AttachShootEnemyBehaviour(Entity, float speed)` | `CreateShootEnemyUnit` |
| | `AttachEnemyTankBehaviour(Entity tank, Entity cannon)` | `CreateEnemyTank` |

Removed along the way:
* `std::shared_ptr<BlackBoard> Board` locals in `CreateTank` / `CreateEnemyTank`
  that are no longer used there.
* The unused local `Entity PlayerBase = *ECS.Visit<PlayerBaseComponent>().begin();`
  in `CreateShootEnemyUnit`.

## Why part of `UIState` is saved

Entering build mode takes 10 crystals right away, and the wall then follows the
cursor (`GameState::ObstacleInCursor`) until it is placed. If only `GameState`
and the entities were saved, a save made while placing a wall would load with
the crystals spent and the wall entity existing, but the UI outside build mode.
The wall would be frozen and could never be placed. Saving the interaction mode
and the wall's `flipped` orientation keeps the two consistent. The mouse and
widget fields in `UIState` are per-frame input and are not saved.

## `BasicEnemyUnit::Speed` (new field)

An enemy soldier's walking speed is random per unit and used to live only inside
its `VectorFieldNode`. It is now also stored on the component
(`CreateShootEnemyUnit` fills it), so a loaded enemy keeps its speed. The field
has a default value, so the existing brace-initialisations (`{100}`, `{200}`)
still compile. The enemy tank doesn't use it.

## `CreateMainLevel.cpp/.h` – `RestoreMainLevelRuntimeState()`

1. Recreates both vector field grids from their saved configuration and applies
   every `AIObstacle` entity (base, crystals, placed walls).
2. Points `BlackBoard::UnitTarget` at the restored selector (`UITarget`).
3. Rebuilds behaviour trees, identifying each unit type from its components:
   * `PlayerControlUnit` with `isTank`: player tank, using the child entity with
     the `CannonTank` mesh as the turret
   * `PlayerControlUnit` with the `SoldierUnitAsset` mesh: soldier
   * `PlayerControlUnit` with the `SupportUnitAsset` mesh: support (miner)
   * `PlayerControlUnit` with the `ObstacleWall` mesh: wall, no AI
   * `BasicEnemyUnit` with the `BasicEnemy` mesh: enemy soldier, using its saved
     `Speed`
   * `BasicEnemyUnit` with a `CannonTank` child: enemy tank

`MainLevel::OnWorldRestored()` calls this function.

## `GameManager` – save / load entry points

```cpp
bool SaveGame(const std::string& path, std::string& error);
bool LoadGame(const std::string& path, std::string& error);
void RequestSave(const std::string& path = QUICK_SAVE_PATH);   // "saves/quicksave.ubsave"
void RequestLoad(const std::string& path = QUICK_SAVE_PATH);
const std::string& GetActiveScene() const;
```

* The save stores the active scene name in the metadata (`Scene = MainLevel`).
* `LoadGame` works in this order:
  1. Read and **fully validate** the file. On failure nothing changes.
  2. `SetActiveScene(savedScene)`: the normal scene-switch path loads assets and
     sets up lights, camera, collision callbacks and systems.
  3. `WorldSerializer::Apply` replaces the freshly built level with the saved
     world.
  4. `Scene::OnWorldRestored()` rebuilds the AI.
* `RequestSave` / `RequestLoad` queue the operation. It runs at the **start of
  the next `Update`**, before any system runs, so it never happens halfway
  through a frame. This makes them safe to call from inside scene or system
  code.
* A status line ("Game saved", "Load failed: …") is drawn for 2.5 s at the
  top-left of the screen.

## Controls

| Key | Action (in the main level) |
|---|---|
| **4** | Quick save to `saves/quicksave.ubsave` |
| **5** | Quick load from `saves/quicksave.ubsave` |
| **Tab** | Return to the scene editor (only during a play test started with the editor's **Play**) |

The keys trigger once per press: holding a key does not save every frame.
Keys 4 and 5 are not used by the game or by the controller emulation in
`AppSettings.h`. The `saves/` directory is created on the first save, relative
to the working directory, which is the repository root as configured by the
existing CMake setup. `saves/` is added to `.gitignore`.
