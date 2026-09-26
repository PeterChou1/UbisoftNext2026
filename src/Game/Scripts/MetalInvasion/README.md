# Metal Invasion on the scene + script engine

This folder rebuilds the original Metal Invasion game on the new engine, as a
reference for how the engine is meant to be used. There is **no game-specific
engine code**: the game is one scene file plus C++ scripts.

- **Play it:** run the **Game** and pick `metal_invasion` in the menu. You can
  also open `metal_invasion` in the **SceneEditor** and press **Play**.
- **Scene file:** `data/scenes/metal_invasion.ubsave`, a plain text scene file
  (open it in any text editor), authored by
  `tests/scenes/SampleScenes.cpp` (`MetalInvasionLevel`). It holds only:
  - the field;
  - the base (a `PlayerBase` model, tag `Base`, static body, script `MIBase`);
  - the scene script `MetalInvasion`.

  Everything else is spawned by the scripts while playing.

## Rules and controls (same as the original)

1. **Spawn:** up to 10 crystal deposits (30 crystals each) appear 10 to 20
   units from the base.
2. **Preparation (60 s):** buy units and walls, and mine crystals. **Enter**
   starts the invasion early.
3. **Invasion:** every 15 s, `SpawnVolume` groups arrive 24 units away. Each
   group is 70% a battalion of 5 enemy soldiers and 30% an enemy tank. When
   the invasion time is over and every enemy is dead, the next round starts
   with 2 more groups per wave and a 15 s longer invasion.
4. **Game over** when the base (1000 HP) is destroyed. **Enter** plays again.

| Input | Action |
|---|---|
| WASD | pan the camera |
| Left click a unit | select its battalion |
| Space + left click | merge the clicked battalion into the selection |
| Left click the ground | selected units walk there (around obstacles) |
| Right click | deselect / close the shop / cancel a wall |
| Left click the base | shop: Soldiers 10, Support 15, Tank 50, Wall 10 crystals |
| R (placing a wall) | rotate it |
| Esc | back to the scene menu (Game) |

## Where the original code went

| Original (deleted) | Now |
|---|---|
| `MainLevel` scene + `CreateMainLevel` | `metal_invasion.ubsave` (authored scene) + `MetalInvasion::OnStart` |
| `GameState` resource | members of the `MetalInvasion` scene script (`MIGame.h`) |
| `GameRoundControllerSystem`, `EnemyControllerSystem` | `MetalInvasion::UpdateRound`, `SpawnCrystals`, `SpawnEnemyWave` |
| `UnitControllerSystem` (select / order) | `MetalInvasion::HandleMouse`, `Select`, `OrderMove` |
| `BuildObstaclesSystem` | `MetalInvasion::Purchase(Wall)`, `MoveWallPreview`, `PlaceWall` |
| `GameCameraController` | `MetalInvasion::UpdateCamera` |
| `MainLevelUI`, `WinScreen` | `MetalInvasion::OnRender` (HUD, shop, game over) |
| `BlackBoard` + behaviour trees (`AINodes`, `PlayerUnits`, `BasicEnemyUnit`) | one script per unit kind (`MIUnits.h`), each a small state machine |
| `BlackBoard::UnitVectorField / EnemyVectorField` | `MI::FlowField` (`MINavigation.h`) on the engine's `VectorField`, owned by the scene script |
| `Prefabs.cpp` | `MIPrefabs.cpp` (`MI::SpawnSoldier`, `SpawnEnemyTank`, `SpawnCrystal`, ...) |
| Components `PlayerControlUnit`, `BasicEnemyUnit`, `PlayerBaseComponent`, `CrystalDeposit` | tag (`SceneObject::Tag`) + script parameters (`Health`, `Battalion`, `Amount`) |
| `Laser`, `Bullet`, `BulletColliders`, `HandleTankProjectiles` | `MI::SpawnLaser` (a shape with the generic `Mover` script), `MIBullet`, `MIExplosion` |
| Tank = root entity + child meshes | hull model + a separate `Turret`-tagged cannon kept on top of it by `MITurret` |

## How it uses the engine

### Objects are ECS entities built from components

Every prefab in `MIPrefabs.cpp` uses `SceneObjects::CreateModel` /
`CreateShape`, the same calls the editor makes. That gives the entity a
`Transform`, `SceneObject` (name + tag), `Mesh` or `Shape2D`, and a
`FragShaderTag`. On top of those:

- `SceneObjects::SetBodyType` adds the `RigidBody`: dynamic for units,
  static for walls and the base, trigger for bullets.
- A `ScriptComponent` names the behaviour and its parameters, e.g.
  `{"MISoldier", {Battalion: 3, Health: 100}}`.
- An `AIObstacle` (an engine component) marks crystals, walls and the base as
  obstacles for path finding.

The render systems, physics and serializer handle these objects without
knowing anything about the game.

### Behaviour is scripts, which the ScriptSystem creates from components

Nothing calls `new MISoldier`. The `ScriptSystem` sees the `ScriptComponent`
on the next frame, creates the registered script (`MIScripts.cpp`), binds its
parameters, and calls `OnStart` / `OnUpdate` / `OnCollisionEnter`. Destroying
the entity ends the script.

### Scripts talk to each other through the engine

- `SceneScriptAs<MetalInvasion>()`: any unit reaches the game, for example to
  get the path finding fields, the current move order, or to add crystals.
- `ScriptOf<MIUnit>(entity)`: reaches another object's script to damage it,
  read its battalion, or mine a crystal.
- `FindByTag("Enemy")`, `PositionOf(e)`: queries over `SceneObject` and
  `Transform`.

### Input

- `KeyDown` / `KeyPressed` for the keyboard.
- `MouseClicked`, `MouseRightClicked` and `MouseGround` for the mouse: the
  last one is the point of the field under the cursor, computed with the
  camera.
- The shop uses the engine's immediate mode widgets (`Button`,
  `DrawContainer`, `FillBar`) from `OnRender`.

### Physics

- Units move by setting their body's velocity (`Walk` / `Halt`), so they push
  each other and are stopped by walls and the base.
- Bullets are trigger bodies: they report `OnCollisionEnter` without pushing
  anything, then explode.

### Rendering

- Selected units switch to the engine's `RedShaderID` by changing their
  `FragShaderTag`.
- A wall that can't be built turns red: its `Shape2D` colour changes and
  `SceneObjects::ShapeChanged` rebuilds its mesh.
- Lasers are thin 2D shapes.

### Saving

Everything that defines the world is in components, so saving a game in the
middle of a round and loading it back restores every unit, wall, crystal and
cannon with its script (see `MetalInvasionTests.cpp`).

The scene script's own counters (crystals, round, timers) live in the script
and restart from the scene parameters. Move them into a component or
resource if saves must keep them.

## Differences from the original

- Behaviour trees became per-script state machines. (The engine's unused
  generic tree nodes, `Engine/BehaviorTree.h`, were later removed.)
- Units are models normalised to a footprint (`MI::SOLDIER_SCALE`, ...), and
  walls are 2D rectangles.
- **Explosions:** one explosion damages everything of the other side within
  1.5 units once (35 HP). Originally it was 1 HP per physics contact while it
  grew. A tank's own death explosion is only visual.
- **Additions:** Enter skips the preparation phase, and right click cancels
  wall placement with a refund.

## Files

| File | Contents |
|---|---|
| `MINames.h` | tags, script names, model names |
| `MIPrefabs.*` | everything the game spawns, and which side an object is on |
| `MINavigation.*` | `MI::FlowField`, path finding around obstacles |
| `MIUnits.*` | unit, tank, base, wall, crystal, bullet and explosion scripts |
| `MIGame.*` | the `MetalInvasion` scene script: rounds, economy, shop, selection, orders, walls, camera, HUD |
| `MIScripts.*` | registers every script with its editor parameters |

The tests in `tests/MetalInvasionTests.cpp` cover:

- rounds, the shop, selection and orders, path finding around walls;
- mining, lasers, tanks and bullets, walls;
- game over and restart, clicking with the mouse, and saving mid-game.
