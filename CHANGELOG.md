# Changelog

## Generic scene editor, C++ scripting and the Engine / Editor / Game split

The Metal Invasion game logic is gone. What is left of it (ECS, 3D CPU renderer,
2D physics, immediate mode UI and the save system) is now a reusable **Engine**.
Two programs are built on top of it:

- the **SceneEditor**, which authors scenes made of basic 2D shapes and 3D models;
- the **Game**, which plays those scenes, driven by C++ scripts.

```
                 +--------------------------------------------------+
                 |  Engine (static library, src/Engine)             |
                 |  ECS · renderer · physics · UI · serialization   |
                 |  World/  scene components, shapes, ScenePlayer   |
                 |  Scripting/  Script, SceneScript, ScriptSystem   |
                 +------------------------+-------------------------+
                                          |
                 +------------------------+-------------------------+
                 |  GameScripts (static library, src/Game/Scripts)  |
                 |  Rotator, Patrol, PlayerController, Collectible, |
                 |  Hazard, Spawner, ... CollectGame (scene script) |
                 +-----------+-------------------------+------------+
                             |                         |
             +---------------+-------+     +-----------+----------------+
             | SceneEditor (exe)     |     | Game (exe)                 |
             | src/Editor            |     | src/Game                   |
             | place / edit shapes,  |     | scene menu + ScenePlayer,  |
             | attach scripts, Play  |     | runs the scripts           |
             +-----------+-----------+     +-----------+----------------+
                         |     data/scenes/*.ubsave    |
                         +------------>----------------+
```

Both programs link the same `GameScripts` library. The editor therefore lists
the real scripts in its inspector and can run them in play mode. The Game plays
exactly what was authored.

---

## 1. How scenes and scripts work with the ECS

The ECS is unchanged and still the single source of truth. A scene *is* an
ECS world. The editor edits components, the serializer saves components, and
scripts read and write components. Nothing keeps a second copy of the world.

### 1.1 A scene object is an entity with components

| Component | Where | What it holds |
|---|---|---|
| `Transform` | engine (existing) | position / yaw / scale; objects live on the XZ plane (`Plane = XZ`) |
| `SceneObject` | `World/SceneComponents.h` | `Name` (unique in the scene) and `Tag` (`"Player"`, `"Pickup"`, ...) |
| `Shape2D` | `World/SceneComponents.h` | `Type` (Rectangle, Circle, Triangle, Polygon), `Width`, `Height`, `Sides`, `Thickness`, `Color`; `Built` is a runtime flag |
| `Mesh` | engine (existing, now by name) | `Model` = file name in `data/models` for 3D model objects |
| `FragShaderTag` | engine (existing) | `ShapeShaderID` for shapes, `BlinnPhongID` for models |
| `RigidBody` | engine (existing) | present when the object has a body: static, dynamic or trigger |
| `ScriptComponent` | `World/SceneComponents.h` | `Script` (registered name) + `Params` (name → float) |

Scene-wide data is an ECS **resource**, `SceneSettings`. It holds the scene
name, the **scene script** name and parameters, the field size and the game
camera.

`SceneObjects::CreateShape / CreateModel` (in `World/SceneObjects.cpp`) is the
only code that assembles these components. The editor, the scripts
(`Spawn(...)`) and the tests all use it, so an object spawned at runtime is
identical to one placed in the editor.

The existing systems pick these components up through ordinary ECS visitors:

- **MeshHandler** visits `Transform + Shape2D`. It builds an extruded prism
  (`ShapeGeometry::BuildMesh`) into the shared vertex / index buffers, and
  rebuilds it when `Built` is reset (`SceneObjects::ShapeChanged`). Through
  `VisitDeleted<Shape2D>` it removes the geometry of destroyed objects.
  Models go through the existing `Transform + Mesh` path, now loaded by name.
- **ShaderHandler** gives shapes the new lit vertex-colour shader
  (`ShapeShaderSIMD`).
- **PhysicsSystem** simulates every `RigidBody + Transform` as before.
  `SceneObjects::SetBodyType` builds the body from the shape: a rectangle, a
  circle, or a polygon from the outline.

### 1.2 Scripts: data in the ECS, behaviour in the ScriptSystem

Scripts are split in two halves:

- **The data is a component.** `ScriptComponent { Script = "Patrol", Params = {Speed: 3} }`
  is a plain, copyable struct. It is saved in the scene file, copied by
  undo / redo, restored by play / stop, and visited like any other component.
- **The behaviour is a C++ object owned by `ScriptSystem`.** It is not stored in
  the ECS, because instances are polymorphic, own runtime state and must never
  be serialized. `ScriptSystem` keeps `Entity → instance` and keeps it in sync
  with the components every frame.

```
ScriptSystem::Update(ms)                    (only while the scene simulates)
 1. Sync objects   for every entity in ECS.Visit<ScriptComponent>() without an
                   instance: ScriptRegistry::Create(name) → Bind(entity, params)
                   → OnStart(). Instances whose entity died, whose component was
                   removed or whose script name changed get OnDestroy().
 2. Sync scene     SceneSettings::SceneScript → the one SceneScript instance
                   (same rules)
 3. Contacts       ColliderCallbackSystem::TakeContactEvents() →
                   OnCollisionEnter / OnCollisionExit on BOTH objects' scripts
 4. Update         scene script first, then object scripts in entity order
                   (deterministic); objects destroyed earlier this frame skipped
ScriptSystem::Render()                      OnRender() (HUD text) after the 3D frame
```

Because instances are derived from components, anything that replaces the
world keeps scripts correct automatically: loading a file, undo / redo, the
editor's Stop, or `ECS.Reset`. `GameManager` calls `ScriptSystem::Reset()`
(`OnDestroy` for everything) on every scene switch and load, and the next frame
recreates instances from the components.

**Scripts reach the ECS only through the normal ECS API.** `ScriptBase` wraps
it in a few helpers:

| Helper | ECS operation |
|---|---|
| `Get<T>(e)`, `Has<T>(e)`, `Resource<T>()` | `ECS.GetComponent / HasComponent / GetResource` |
| `Self()`, `GetTransform()`, `Position()`, `SetPosition()`, `Yaw()`, `SetYaw()` | the script's own `Transform` |
| `Body()`, `SetVelocity()` | the script's own `RigidBody` |
| `Find(name)`, `FindByTag(tag)`, `NameOf(e)`, `TagOf(e)` | queries over `SceneObject` |
| `Spawn(ShapeDesc)`, `Destroy(e)`, `DestroySelf()` | `SceneObjects::CreateShape` / `ECS.DestroyEntity` (children too) |
| `Param("Speed")` | value from the `ScriptComponent` (or `SceneSettings`) merged over the declared defaults |
| `SceneScriptAs<CollectGame>()` | the running scene script, so objects can report to it |
| `KeyDown`, `KeyPressed` | `Input` (edge-detected keyboard, updated once per frame) |
| `LoadScene(name)`, `RestartScene()` | deferred requests handled by `GameManager` **between** frames |

A spawned entity gets its script on the **next** frame's sync, the same way a
placed one does. A destroyed entity's script gets `OnDestroy` in the next sync.
The `MeshHandler` and `PhysicsSystem` see the destruction through
`VisitDeleted` as usual.

**Collisions come from the existing physics system.** `PhysicsSystem` reports
every overlapping pair to the `ColliderCallbackSystem` resource
(`SubmitContact`). Each physics step, that resource compares the pairs with the
previous step's and produces **Enter / Exit** events. Triggers
(`RigidBody::Collidable = false`) produce events but no collision response.
That is how pickups and hazards work.

**Rule for saved state:** gameplay state that must survive a save belongs in
components or resources. Script members are runtime state, and they are
recreated from the component parameters when a scene is loaded.
`CollectGame`'s score and lives are deliberately per-run: a scene file is an
authored level, not a mid-game save.

### 1.3 Frame order (`GameManager`)

```
Update(ms)
  ProcessRequests()      pending save / load / restart / scene change (scripts ask, never switch mid-frame)
  Input::Update()        key edges for the frame; Tab toggles the renderer
  if scene simulates:    ScenePlayer always, SceneEditor only while playing
     PhysicsSystem       → contact events
     ParticleSystem
     ScriptSystem        sync → contacts → OnUpdate
  UIStateManager, active Scene::Update (editor GUI input, menu, Esc)
  ShaderHandler, MeshHandler (build / rebuild / delete geometry)
Render()
  vertex → clip → raster → fragment (3D renderer, unchanged)
  ScriptSystem::Render (HUDs), Scene::Render (editor / menu UI)
  MeshHandler::DeleteDestroyed + ShaderHandler::HandleShaderDelete
ECS.FlushECS()           end of frame (in the program's Render)
```

### 1.4 Scene files

A scene file is a normal save made by `WorldSerializer` (same binary,
versioned, checksummed format). It uses the metadata
`Scene = "Play"` (`ScenePlayer::NAME`), `Name`, and `Tool = "SceneEditor 2"`.

- `GameManager::LoadGame(path)` switches to the `ScenePlayer`, applies the
  world, applies the scene camera, and resets the scripts.
- The Game's menu lists `data/scenes/*.ubsave`.
- `CollectGame` loads `level_<N+1>` when a level is complete.

Serializers are registered in `Serialization/SceneSerialization.cpp`: the
engine components, `SceneObject`, `Shape2D`, `ScriptComponent` and the
`SceneSettings` resource. Render state (`Shape2D::Built`, `Mesh::Loaded`, shader
ids) is reset on load so the render systems rebuild it.

---

## 2. Writing a script

```cpp
// src/Game/Scripts/MyScripts.h
class Bouncer : public Script
{
  public:
    void OnUpdate(float dt) override
    {
        SetYaw(Yaw() + Param("Spin") * dt);
    }
    void OnCollisionEnter(Entity other) override
    {
        if (TagOf(other) == "Player")
            if (auto* game = SceneScriptAs<CollectGame>())
                game->AddScore(1);
    }
};

// src/Game/GameScripts.cpp, inside RegisterGameScripts()
r.Register<Bouncer>("Bouncer", "Spins, scores when touched", {{"Spin", 90.0f, 15.0f}});
```

The parameter declaration gives the name, the default and the editor step. A
class derived from `SceneScript` instead of `Script` is a scene script: it
appears in the editor's **Scene** tab instead of the object inspector.
Registration is explicit (`RegisterGameScripts()`), so static-library linking
never drops a script.

Scripts in `GameScripts`:

| Script | Kind | Parameters | Behaviour |
|---|---|---|---|
| `Rotator` | object | Speed | spins (degrees / s) |
| `Patrol` | object | Distance, Speed | back and forth along its facing, starting where it was placed |
| `Mover` | object | Speed, Lifetime | moves forward, destroyed after its lifetime or off the field |
| `PlayerController` | object | Speed | WASD / arrows; sets the velocity of a dynamic body, else moves the transform |
| `Follower` | object | Speed, Range | chases the `Player` tag within range |
| `Collectible` | object | Points, SpinSpeed | on Player contact: adds score to `CollectGame`, destroys itself |
| `Hazard` / `MovingHazard` | object | – / Distance, Speed | on Player contact: `CollectGame::PlayerHit()` (or restarts the scene) |
| `Projectile` | object | Speed, Lifetime | a `Mover` that hurts the Player |
| `Spawner` | object | Interval, Speed, Lifetime | spawns trigger circles running `Projectile` |
| `CollectGame` | **scene** | Level, Lives | collect every `Pickup`; lives, respawn, HUD, loads `level_<Level+1>` or shows the win screen |

---

## 3. Scene editor (SceneEditor target)

- **Palette:** Select, Rectangle, Circle, Triangle, Polygon and Model (keys 1-5).
  Click the field to place an object. The brush sets size, sides, rotation,
  body type, tag, model and colour. Snap-to-grid is toggled with G.
- **Viewport:** click to select; drag to move (one undo step per drag).
  Right click stops placing. WASD pans, Z / C zooms.
- **Object inspector:** size / height / sides / thickness, colour, body
  (None, Static, Dynamic, Trigger), tag, model and scale, script and its
  parameters. Buttons rotate (R), duplicate (F) and delete (X).
- **Scene tab:** scene script and its parameters, field size, and
  "Game camera = view". It also shows a live validation report.
- **Toolbar:** scene list (`data/scenes` plus `my_scene_1..3` slots), New,
  Load, Save, Undo / Redo (U / Y) and Play / Stop (P).
- **Play** first validates the scene: unique names, objects on the field, and
  known scripts. It then runs physics and scripts inside the editor. **Stop**
  restores the scene exactly (serializer snapshot).
- All editing logic lives in the headless `Editor::SceneEditor`. The GUI
  (`SceneEditorScene`) only turns input into calls on it, and the tests and the
  scene authoring tool drive the same API.

## 4. Game (Game target)

A menu lists the scenes in `data/scenes`; click one to play it. **Esc** returns
to the menu, **Tab** switches renderer and **Q** quits (ContestAPI).
The committed scenes are:

- `level_1` and `level_2`: `CollectGame` levels;
- `sandbox`: every shape, crates, a chaser and models;
- `empty`.

## 5. Build

| Target | Type | Sources |
|---|---|---|
| `Engine` | static library | `src/Engine/**` |
| `GameScripts` | static library | `src/Game/Scripts/**`, `src/Game/GameScripts.*` |
| `Game` | executable | `src/Game/GameMain.cpp`, `SceneMenu.*` |
| `SceneEditor` | executable | `src/Editor/*` |
| `EngineTests`, `AuthorScenes` | headless tests / tool | `tests/` (option `BUILD_TESTS`, or `cmake -S tests`) |

- MacOS: `make run` runs the Game and `make run_editor` runs the SceneEditor.
- Windows: both executables get the repository root as their debugger working
  directory and a freeglut DLL copy, and Game is the startup project.
- The option `BUILD_SERIALIZATION_TESTS` is now `BUILD_TESTS`.
- Models moved from `data/` to `data/models/`.

---

## 6. Removed: Metal Invasion game logic

Deleted:

- **Game code:** AI (AINodes, AISystem, BehaviorTree nodes, BlackBoard*),
  units, enemies, bullets, lasers, crystals, player base, prefabs, obstacle
  building, round controller, game state, camera controllers, main level,
  title / options / win screens and UI, `GameUtils`, `GameTest.cpp`.
- **Serialization and editor:** the Metal Invasion serialization
  (`GameSerialization.*`) and the old prefab-based editor.
- **Assets and tests:** the Metal Invasion scenes and their tests.

The engine pieces they used were kept and made generic:

- `BehaviorTree.h` keeps the generic nodes (sequence, selector, repeat,
  concurrent, `Wait`), usable from scripts.
- `ColliderCategory` now has generic categories.
- The asset server loads models by name instead of the old `ObjAsset` enum.

## 7. Engine fixes found along the way

- **Physics:** the accumulator subtracted `MaxTime` instead of `StepTime`, so
  physics ran at about a third of real time.
- **Assets:**
  - `AssetServer::GetMaterial` asserted `-1 <= size` (a signed / unsigned
    comparison that always fails), so meshes without a material crashed debug
    builds.
  - `SetVertShader` removed the fragment shader.
  - Models are now normalised on load: centred, standing on y = 0, with a
    1-unit footprint. The source files range from 0.16 to 200 units, so a
    scale of 1 now means the same size for every model.
- **Rendering:**
  - The hardware-triangle render path (the default "LineRendering" mode) drew
    every triangle in its `.obj` material colour. Shapes now use their vertex
    colours, lit like the software shader.
  - Geometry of entities destroyed during `Render` (the editor's Delete
    button, a script's `OnRender`) was never removed from the vertex buffer,
    because the ECS flushes before the next `Update`.
    `MeshHandler::DeleteDestroyed()` now also runs at the end of `Render`.
- **Transforms:** `Transform::Inverse` is a rigid inverse (a transpose), which
  is wrong for scaled objects. Point-in-object tests (picking) now project on
  the affine axes instead.
- **Scene flow:** `SetActiveScene` now forgets the previous scene file, so a
  restart can no longer reload the wrong scene.
- **Other additions:**
  - `RigidBody(std::vector<Vec2>)` (polygon bodies) is now implemented, with
    area and inertia.
  - `Shape::CreatePolygon` accepts either winding.
  - `Input` provides key edges (pressed / released) for scripts and the editor.

## 8. Tests (`tests/`, 104 tests, headless)

The whole engine, editor, scripts and scene menu are compiled against a
stubbed ContestAPI (`tests/support/AppStub`). The stub records printed text,
triangles and every presented pixel, and lets tests drive the mouse and keys.
`TestEnvironment` starts the engine exactly like the programs do, and every
frame runs the real `GameManager::Update` / `Render`.

| File | Covers |
|---|---|
| `SerializationTests` | world round trips of every scene feature, rewind, ids, visitors, byte-identical re-save, corruption / truncation / version / layout robustness, compatibility, files |
| `ShapeGeometryTests` | counter-clockwise outlines, outward-facing triangles, containment (incl. rotated / scaled), yaw vs `Facing`, bodies per shape and body type |
| `ScriptingTests` | registry, lifecycle, missing / wrong-kind scripts, scene script, contacts from real physics, triggers vs solids, spawning, input edges, `LoadScene`, restart |
| `GameScriptsTests` | every game script: Rotator, Patrol, PlayerController (kinematic / dynamic), Follower, Collectible + CollectGame score, level transition to `level_2`, hazards / lives / game over / restart, Spawner / Projectile |
| `EditorTests` | editor core: new scene, placing every kind, unique names, clamping, edits, pick, duplicate, scripts, validation, save / load, undo / redo, play snapshot |
| `EditorGuiTests` | the GUI through mouse / keys: palette placing, brush steppers, drag, inspector, shortcuts, scene tab, Play / Stop, scene list + Save / Load |
| `RenderTests` | shape colours at their projected pixels (software renderer), deletion, models over shapes, hardware path colours, model normalisation, Tab toggle |
| `SceneFileTests` | committed `data/scenes` load cleanly, match `tests/scenes/SampleScenes.cpp` byte for byte, and play without missing scripts |
| `ArchiveTests`, `CameraPickingTests` | unchanged low level tests |

The suite also passes under GCC with AddressSanitizer / UndefinedBehaviorSanitizer
and under clang. The fixes in section 7 were mutation-checked: reverting each
one makes a test fail.

## 9. File summary

**Engine – new**

- `World/SceneComponents.h`: `SceneObject`, `Shape2D`, `ScriptComponent`, and the `SceneSettings` resource.
- `World/ShapeGeometry.*`: shape outlines, prism mesh building, point containment.
- `World/SceneObjects.*`: creating objects, body types, yaw / position helpers, lookups by name and tag, recursive destroy, containment, outlines, camera.
- `World/ScenePlayer.*`: the scene that plays authored scenes (camera from `SceneSettings`, Esc exits).
- `Scripting/Script.*`: `ScriptBase`, `Script`, `SceneScript`, and the ECS helpers.
- `Scripting/ScriptRegistry.h`: name → factory and declared parameters.
- `Scripting/ScriptSystem.*`: instance sync, contacts, update and render.
- `Input.*`: keyboard state and edges.
- `ShapeShaderSIMD.*`: lit vertex-colour shader.
- `Serialization/SceneSerialization.*`: scene serializers and the registry.
- `EngineGlobals.cpp`: the global `ECS` and `GameSceneManager`.

**Engine – changed**

- Scene flow:
  - `GameManager.*`: script system, frame order, scene-file load / save / restart requests, render cache reset, Tab renderer toggle.
- Assets and meshes:
  - `AssetServer.h`: models by name, lazy and normalised, `AvailableModels()`, `ShapeShaderID`, `GetMaterial` / `SetVertShader` fixes.
  - `Assets.h`: removed the `ObjAsset` enum, added `ShapeShaderID`.
  - `Mesh.h`: model by name.
  - `MeshHandler.*`: `Shape2D` meshes, model swap, `DeleteDestroyed()`.
- Rendering:
  - `RasterizerSystem.cpp`: vertex colours in the hardware path.
- Physics:
  - `PhysicsSystem.cpp`: contact reporting, accumulator fix.
  - `ColliderCallbackSystem.*`: Enter / Exit contact events.
  - `RigidBody.*`: polygon bodies, `IsStatic()`.
  - `Shape.h`: `CreatePolygon`.
- UI:
  - `Widget.*`: `ColorSwatch`.
- Generic cleanup (Metal Invasion references removed):
  - `BehaviorTree.h`: generic nodes and `Wait`.
  - `ColliderCategory.h`: generic categories.
  - `DebugPhysicsRenderer.cpp`: no BlackBoard.
  - `Serialization/EngineSerialization.h`: `Mesh` by name, no UITarget.
- Moved: every other engine file moved from `src/Game` with `git mv`, unchanged.

**Editor (`src/Editor`)**

- `SceneEditor.*`: the headless editor core.
- `SceneEditorScene.*`: the GUI.
- `EditorMain.cpp`: the program entry points.

**Game (`src/Game`)**

- `Scripts/*`: the scripts.
- `GameScripts.*`: `RegisterGameScripts()`.
- `SceneMenu.*`: the scene list.
- `GameMain.cpp`: the program entry points.

**Data**

- `data/models/`: the models, moved from `data/`.
- `data/scenes/{empty,level_1,level_2,sandbox}.ubsave`: generated by `author_scenes`.

**Tests**

- `tests/CMakeLists.txt`: the `HeadlessEngine` library, `EngineTests`, and `AuthorScenes` / `author_scenes`.
- `support/`: the App stub (with pixel, triangle and text capture), `TestEnvironment` and `WorldFixture`.
- `scenes/SampleScenes.*`: the sample scenes.
- `tools/AuthorScenes.cpp`: the scene authoring tool.
- The test files listed in section 8.
