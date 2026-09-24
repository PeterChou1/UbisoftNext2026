# 05 – Scene editor (authoring GUI) and committed scenes

A scene authoring tool for Metal Invasion levels, built on the engine's
immediate-mode widgets. Authored scenes are saved with the save system to
`data/scenes/`, committed on this branch, and tested on every test run.

## Using it

**Title screen → Scene Editor.**

```
+-------------------------------------------------------------------------+
| [scene list v]  New  Load  Save  Undo  Redo  Play  Menu   Editing x *   |
+-----------+-------------------------------------------------+-----------+
| PALETTE   |                                                 | INSPECTOR |
|  Select   |                                                 | kind, pos |
|  1 Soldier|           top-down 3D view of the level         | health    |
|  2 Support|         (play area drawn as a blue square)      | battalion |
|  ...      |                                                 | crystals  |
| BRUSH     |                                                 | Rotate/Del|
|  HP  - +  |                                                 | SCENE     |
|  ...      |                                                 |  settings |
+-----------+-------------------------------------------------+-----------+
| status message / key hints                                              |
+-------------------------------------------------------------------------+
```

| Action | Mouse / key |
|---|---|
| Pick a prefab to place | palette button or **1–7** (Soldier, Support, Player Tank, Enemy, Enemy Tank, Crystal, Wall) |
| Place | left click on the ground (repeatable) |
| Back to the select tool | **Space**, right click, or the **Select** button |
| Select / move | left click an object, drag it |
| Rotate selected | **R** or *Rotate* (45° steps; walls toggle between their 2 orientations) |
| Delete selected | **X** or *Delete* (the base and unit selector can't be deleted, the game needs them) |
| Undo / redo | **U** / **Y** or the toolbar |
| Pan / zoom | **W A S D** / **Z** (in) **C** (out) |
| Grid snap (0.5) | **G** or the *Snap* checkbox |
| Brush | values used for the next placed object: health, battalion, crystal amount, enemy speed, rotation |
| Inspector | edit the selected object's health / battalion / crystals / speed with the – + buttons |
| Scene settings | starting crystals, round number, spawn volume (`GameState`) |
| Scene slot | pick a file in the scene list (every file in `data/scenes` plus `my_scene_1..3`), then **Save** or **Load** |
| Play | saves `saves/editor_playtest.ubsave` and loads it into the main level with full AI |
| Back to the editor | **Tab** in the main level during a play test: the editor reopens the scene you were playing |
| Menu | back to the title screen |

The toolbar shows `*` next to the scene name when there are unsaved changes.
The inspector shows *Playable*, or the number of problems that would prevent the
scene from being played.

While the editor is open the world is **frozen**. The new
`Scene::SimulatesWorld()` returns `false`, so `GameManager` skips physics,
particles and AI (units would otherwise be pushed apart or start walking), while
rendering and UI keep running.

## Architecture

```
SceneEditorScene (GUI, src/Game/Editor)      input + drawing only
        │ calls
        ▼
Editor::SceneEditor (headless core)          place, pick, move, rotate, delete,
        │                                    properties, validation, undo/redo,
        │ uses                               save/load scene files
        ├──► Prefabs (src/Game/Prefabs.*)    builds each object exactly as the game does
        └──► WorldSerializer                 scene files + undo snapshots
```

* **`Prefabs`** is one builder per placeable object: ground, base, selector,
  soldier, support, player tank (root + base/cannon children), enemy, enemy tank,
  crystal, placed wall. The game's own spawn functions (`CreateSoldierUnits`,
  `CreateTank`, `CreateShootEnemyUnit`, `CreateEnemyTank`, `CreateCrystal`,
  `CreateMainLevel`) now call these, then attach AI. The editor therefore can't
  drift from what the game spawns, and `RestoreMainLevelRuntimeState()`
  recognises every authored object when a scene is played. The refactor keeps
  the game's behaviour identical, including existing quirks (for example
  `CreateEnemyTank` ignores its `health` argument).
* **`Editor::SceneEditor`** holds all editing logic, has no rendering or input
  dependencies, and is fully unit tested.
  * Placement is clamped to the play area.
  * Picking uses oriented rectangles for physics shapes, so a long wall can be
    picked along its length but not beside it.
  * **Undo/redo snapshots the world with the serializer** (64 steps). Every edit
    is therefore also a save + load round trip of the whole world.
  * `Validate()` requires exactly one player base, exactly one unit selector,
    and everything inside the play area. `SaveScene` refuses unplayable scenes.
    `LoadScene` accepts only main-level scenes and keeps the current scene if
    the file is invalid.
* **Play test round trip**: `GameManager::BeginPlaytest(path, "SceneEditor")`
  loads the scene into the main level and remembers the editor. **Tab** calls
  `EndPlaytest()`, which switches back at the start of the next frame, never
  in the middle of the main level's update. The editor's `Setup` then reopens
  the play-tested file. Entering the editor any other way starts a new scene,
  and leaving the main level any other way cancels the play test.
* **`SceneEditorScene`** turns mouse and keys into editor calls and draws the
  panels. When the world is replaced (new, load, undo, redo) it drops the
  per-entity render caches (vertex/index buffers, `RenderConstants` maps) so the
  renderer rebuilds them. Normally `ECS.Reset()` does this on a scene switch.

## Scene files on this branch

`data/scenes/*.ubsave` are regular save files with metadata
`Scene = MainLevel`, `Name = <scene>`, `Tool = SceneEditor 1`. They are playable
through `GameManager::LoadGame`.

| File | Content |
|---|---|
| `empty_arena` | Minimum playable scene: ground, base, selector |
| `first_contact` | 3×3 soldier battalion, 2 miners, 3 crystals, 6 enemies |
| `fortress` | Base walled in with both wall orientations, a tank on guard, 60 starting crystals |
| `tank_battle` | 2 player tanks vs 3 enemy tanks and 8 enemies, round 3 |
| `crystal_rush` | 7 deposits (10–70 crystals), miners; authored using undo/redo and property edits |

They are authored **through the editor API**, the same calls the GUI makes, by
`tests/scenes/SampleScenes.cpp`, and written by the `AuthorScenes` tool:

```bash
cmake --build build/tests --target author_scenes     # (re)writes data/scenes/<sample>.ubsave
```

### How the scene files test serialization

`tests/SceneFileTests.cpp` runs on every test run:

1. **Golden test**: each sample is authored again with the current code. The
   committed file must load into exactly the same world (field-by-field
   comparison) and be **byte-identical** to the fresh file. Any change to the
   save format, a `Serialize` function or a prefab shows up here. If a change is
   intentional, run `author_scenes` and commit the new files.
2. **Every `*.ubsave` in `data/scenes`**, including scenes you save from the
   GUI and commit, must load without warnings, be playable, re-save to exactly
   the same bytes, and survive a clear + load cycle unchanged. Saving a new scene
   from the editor into `data/scenes/` and committing it automatically adds it
   to the suite.
3. The sample scenes contain what their descriptions promise (counts, wall
   orientations, round settings, the result of the undo/redo authoring steps).

## Engine fixes found while building the editor

| Fix | File | Effect |
|---|---|---|
| Mouse → ground ray casting is now the exact inverse of the renderer's projection | `Camera.cpp` (`ScreenSpaceToWorldPoint`) | The old ray used world up instead of the tilted camera's up, so clicks landed several units off away from the screen centre (for example (12, 15) → (15.6, 9.3)). The error is now below 0.1 (pixel rounding). **This also changes gameplay**: wall placement and unit move orders in the main level now land exactly under the cursor. Worth a quick check when you play |
| Shader cleanup erased from copies of the maps | `ShaderHandler.cpp` (`HandleShaderDelete`) | `auto EntToFragType = ...` copied the maps, so entries were never removed. They are now references |
| Deleting an entity whose shader was never initialized asserted | `ShaderHandler.cpp` | Skipped instead. It happens when an object is placed or undone and deleted before the next shader update |
| New scene kept AI data from the previous game | `SceneEditor::NewScene` | Vector field grid sizes and `EnemyTankTargets` leaked into new scenes (found by the golden test) |

## Tests (41 new, 87 total, 1050 checks)

* **`SceneEditorTests.cpp` (20):** new scene, stale-state reset, every prefab
  matches the game, clamping, picking (nearest, oriented walls, tank children),
  moving hierarchies, rotation and wall snapping, removal rules, property
  editing, exact undo/redo of every edit type, redo invalidation, undo bound,
  world-replaced callback, validation, save/load, rejecting non-scene and
  unplayable files, AI recognisability.
* **`SceneEditorGuiTests.cpp` (16):** drives the real `SceneEditorScene`
  headlessly. `tests/support/AppStub.cpp` implements the ContestAPI functions
  (drawing is a no-op, input is scripted) and frames run in `GameManager`
  order. The tests click buttons by their label, click and drag in the 3D
  viewport, and press keys. They cover:
  * placement accuracy across the view, including after panning and zooming
  * panels blocking viewport clicks
  * drag creating exactly one undo step
  * shortcuts, grid snapping, and the undeletable base
  * brush, inspector and scene steppers
  * the toolbar actions, and the scene list not clicking through to the palette
  * Play, Menu, returning from a play test, and render-cache reset
* **`SceneFileTests.cpp` (4):** see above.
* **`CameraPickingTests.cpp` (1):** screen ↔ ground round trip for the
  main-level camera, the editor camera (panned and zoomed) and a top-down view.
  The original ray cast fails it.

The editor's tests were checked by injecting bugs, the same way as the save
system's. Each of these was caught:
* picking ignoring rotation
* place without undo
* wrong wall footprint or collider
* the `NewScene` leak
* a stale committed scene
* dropdown click-through
* Play not remembering to reopen the scene
* one undo per drag frame
* panels not blocking clicks
* no snapping
* no render-cache reset
* the old camera ray cast

The first run of this showed that snapping and the cache reset were untested;
both tests were added.

## Limitations

* The GUI was verified through the headless harness, not on screen. Layout,
  colours and the feel of dragging should get a look on Windows or MacOS.
* Scene names come from a fixed list: existing files plus three `my_scene_N`
  slots. The widget library has no text input. Rename a file in `data/scenes`
  to give it a new name.
* There is no multi-select or box selection. There is also no copy/paste yet.
* The GameManager side of the play test round trip (`BeginPlaytest` /
  `EndPlaytest` / Tab in the main level) is compiled but only exercised through
  stubs, because the main level needs the renderer and AI.
