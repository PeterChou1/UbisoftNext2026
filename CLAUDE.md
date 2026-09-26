# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A C++17 game engine written for the Ubisoft NEXT 2026 programming contest, on top of the contest's
`ContestAPI` (window, input, 2D drawing via GLUT / SDL3). It builds two programs from one engine:

- **SceneEditor** (`src/Editor`): a Unity-style editor for scenes made of 2D shapes, `.obj` models and
  empty transforms, with a hierarchy, prefabs, physics bodies, reflected components and C++ scripts.
- **Game** (`src/Game`): a scene menu plus a player that runs the authored scenes
  (`data/scenes/*.ubsave`) with their scripts. Metal Invasion (`src/Game/Scripts/MetalInvasion`) is the
  reference game.

**Never modify `src/ContestAPI/`.** The user asked for this explicitly. Reading it is fine.

## Building and testing

The real programs only build on Windows (Visual Studio, `generate-windows.bat` → `build/win64`) and macOS
(`./generate-macos.sh`, then in `build/macos`: `make all`, `make run`, `make run_editor`). Both must run
from the repository root to find `data/`.

On Linux (and in cloud sessions) use the **headless test build**. It compiles the whole engine, editor
and game scripts against a stub ContestAPI (`tests/support/AppStub.cpp`, `tests/support/stubs/app.h`) that
records drawn lines and printed text and presents rendered frames into a pixel buffer:

```sh
cmake -S tests -B build/tests && cmake --build build/tests -j4
./build/tests/EngineTests                 # all tests (run from the repo root)
./build/tests/EngineTests "Editor GUI"    # only tests whose name contains the filter
ctest --test-dir build/tests --output-on-failure
```

- Sources are globbed, so re-run the `cmake -S tests ...` configure step after adding or removing files.
- The test framework is home-grown (`tests/support/TestFramework.h`): `TEST_CASE("Area: what it checks")`,
  `CHECK`, `REQUIRE`, `CHECK_EQ`, `CHECK_THROWS_AS`. Test names start with an area prefix ("Physics:",
  "Editor GUI:", "Colliders:" …), which is what the name filter matches.
- `cmake --build build/tests --target author_scenes` regenerates the sample scenes and prefabs in
  `data/scenes` and `data/prefabs` from `tests/scenes/SampleScenes.cpp`, using the headless editor. If a
  change isn't meant to alter scenes, `git status` must show `data/` unchanged afterwards (the output is
  deterministic).
- Physics benchmark: build with `-DCMAKE_BUILD_TYPE=Release` into its own build dir, then
  `cmake --build <dir> --target physics_benchmark`, or run `PhysicsBenchmark [max bodies] [frames] [auto|sweep|grid|all]`.
  Its checksum column must stay identical unless you mean to change the simulation.
- Formatting: `.clang-format` at the root (LLVM based, 4 spaces, 100 columns, braces on their own lines).
  Run `clang-format -i` on the files you change.

### Windows compatibility without Windows

The headless build cannot catch MSVC / MinGW problems in platform code (`#ifdef _WIN32`, `app.h`,
`windows.h` min/max macros; CMake defines `NOMINMAX` for all targets). When touching engine / editor /
game sources, syntax-check them with MinGW if it's installed, e.g.:

```sh
x86_64-w64-mingw32-g++ -std=c++17 -fsyntax-only -DNOMINMAX -DBUILD_PLATFORM_WINDOWS=1 -DBUILD_PLATFORM_APPLE=0 \
  -Isrc/ContestAPI -Isrc/ContestAPI/vendor/glut/include -Isrc/Engine -Isrc/Game -Isrc/Editor <file.cpp>
```

MinGW lacks some Windows SDK headers (e.g. `XInput.h`), so you may need small shim headers on the
include path. Put them in a scratch directory, not in the repo.

## Architecture

### Frame and program structure

`GameMain.cpp` / `EditorMain.cpp` implement the ContestAPI callbacks (`Init`, `Update`, `Render`). Both call
`ECS.Init()`, `GameSceneManager.Setup()` and `RegisterGameScripts()`, then register their `Scene`
subclasses: `SceneEditorScene`, or `SceneMenu` + `ScenePlayer`. `ECS` and `GameSceneManager` are globals
defined in `src/Engine/EngineGlobals.cpp`; engine code refers to them with `extern`.

`GameManager` (`src/Engine/GameManager.cpp`) owns every system. It runs one frame as:

1. `Update`:
   - pending save / load requests;
   - input and UI state;
   - if the active scene `SimulatesWorld()` (the player, or the editor in play mode): physics, particles,
     scripts;
   - the scene's own `Update`;
   - scene lighting;
   - shader and mesh bookkeeping.
2. `Render`:
   - the software pipeline, in order: vertex shading → clipping → tiled rasterization → fragment shading;
   - script HUDs, then the scene's UI;
   - resetting the per-frame buffers.

**Tab** switches between the software rasterizer (fragment shaders, shadow maps) and hardware triangles
(`GameOptions::LineRendering`).

### ECS (`src/Engine/ECSManager.h` and friends)

- Entities are indices below `MAX_ENTITIES` (5000).
- Each component type is stored in a fixed-size array, so component references stay valid while entities
  are added.
- `ECS.Visit<A, B>()` returns a copy of the matching entity set.
- Resources are singletons registered in `GameManager::Setup()` and fetched with `ECS.GetResource<T>()`
  (camera, buffers, lighting, `GameOptions`, `SceneSettings`, …).
- Entity destruction is deferred until `ECS.FlushECS()`, which the programs call after each `Render`.
  Headless code that creates bodies must also flush before systems see them.

### Scene objects, the editor and play mode

- **`World/SceneObjects`** is the single API that builds and changes scene objects:
  - shapes, models, empties;
  - hierarchy, physics bodies, collider shapes, shaders.

  The editor, scripts spawning at runtime and tests all go through it, so objects are built from the
  same components everywhere.
- **Object components** (`World/SceneComponents.h`):
  - `SceneObject` (name, tag);
  - `Shape2D`, or `Mesh` for models;
  - `Transform` (parent / children);
  - optionally `RigidBody`, `ColliderShape`, `ScriptComponent`, `PrefabLink`, `GameCamera`, `SceneLight`
    and reflected game components.
- **The editor** is split into a headless core and a GUI:
  - `Editor::SceneEditor` (`SceneEditor.cpp`) is the core. It does every edit, undo / redo, validation,
    file I/O and play mode, and tests drive it directly.
  - `SceneEditorScene` (plus `EditorInspector.cpp`, `EditorLeftPanel.cpp`, `ContextMenu`) is the
    immediate-mode GUI on top.
  - An edit goes through the core so it records **one undo step**. Undo and play mode snapshot the whole
    world with the serializer, so every edit also exercises a full save and load.
- **`ScenePlayer`** loads a `.ubsave` scene and simulates it. `SceneCamera` / `SceneLight` turn the
  scene's camera and light objects into the renderer's camera and lighting each frame.
- **Prefabs** (`World/Prefab`) are captured object groups saved as `.ubprefab`. Instances carry a
  `PrefabLink`.

### Scripting (`src/Engine/Scripting`, `src/Game/Scripts`)

- Scripts are C++ classes derived from `ScriptBase`, or `SceneScript` for per-scene logic.
- They're registered by name, with editable float parameters, in `RegisterGameScripts()`
  (`src/Game/GameScripts.cpp`).
- Objects refer to scripts **by name** through `ScriptComponent` (name + params), which is what scene files
  store.
- `ScriptSystem` creates the instances when the world simulates and calls their hooks (start, update,
  collision, render).
- Script names come from `Scripts/ScriptNames.h`. Renaming one breaks existing scenes.

### Reflection and serialization

- **Reflected components:** a component is described once, with `REFLECT(Type) { Field("Name", &Type::Member)... }`
  (`Reflection/Reflection.h`). It's then registered in `ComponentCatalog`, e.g. in
  `RegisterGameComponents()`.
- **What registration gives you:**
  - an editor inspector;
  - add / remove in the editor;
  - copying for duplicates and prefabs;
  - by-name saving in scene files (it also registers with the scene serialization registry).
- **Engine components** (`Transform`, `RigidBody`, `Mesh`, …) have hand-written `Serialize` functions in
  `Serialization/EngineSerialization.h`. They are registered in `Serialization/SceneSerialization.cpp`.
- **`WorldSerializer`** saves the whole world in two forms:
  - a versioned binary format with chunks and a CRC;
  - a readable text format (`TextArchive`).

  The two convert into each other.
- **Formats are compatibility contracts.** Registered names, field names / order / types and versions
  determine whether existing `data/` files load. Change a layout only by bumping the registered version
  and handling the old one (see the comment in `SceneSerialization.cpp`).
- `docs/ComponentsTutorial.md` walks through adding a component end to end.

### Physics (`PhysicsSystem`, `RigidBody`, `Collision`, `SAT`, `Manifolds`)

2D impulse physics on the XZ plane (Y is up; a body's angle is the object's yaw, `Angular = -pitch`):

- **Step:** a fixed 16 ms step made of 10 sub-steps.
- **Broad phase** (`PhysicsSystem::BroadPhase`):
  - sort-and-sweep for small scenes, a uniform grid from 256 bodies;
  - candidate pairs are sorted back into all-pairs order, so every mode is **bit-identical** (asserted by
    `tests/PhysicsTests.cpp`).
- **Narrow phase** runs in parallel for large pair counts; its results are applied in order.
- **Transform write-back:** transforms are written once per step. When category collision callbacks are
  registered (`ColliderCallbackSystem`), they are written every sub-step instead, since callbacks may read
  them.
- **Body types:** static bodies have `InvMass == 0`; triggers have `Collidable == false`.
- **Collider shapes:** chosen by `ColliderShape` (Auto / Box / Circle / Polygon, with a scale). Bodies are
  rebuilt with `SceneObjects::SetBodyType` / `ShapeChanged`.
- **Debug outlines:** `World/PhysicsGizmos` computes the collider outlines the editor draws.

### Rendering

A multithreaded CPU renderer:

- **Vertex stage:** per-mesh vertex shaders fill the `VertexBuffer`.
- **Clipping:** `ClipperSystem` clips triangles into screen tiles (`Tiles`), for the camera and for the
  shadow map.
- **Rasterization:** `RasterizerSystem` works per tile, into the `DepthBuffer` and `PixelBuffer`.
- **Fragment stage:** `FragmentShaderSystem` runs SIMD fragment shaders (`*SIMD.cpp`, 8 pixels at a time
  via `SIMD.h`) and presents the `ColorBuffer` through `App` drawing.
- **Shader choice:** shaders are chosen per object with `FragShaderTag` / `VertShaderTag`. `ShaderLibrary`
  maps shader IDs to user-visible names, and `AssetServer` owns the shader instances and loaded models.
- **Shadows:** shadow maps come from the scene light (directional = orthographic, spot = perspective).
  They are sampled with PCF and normal-offset / slope bias in `ShadowSampling`.

## Conventions and constraints

- **Naming:**
  - PascalCase for types, functions and public members;
  - `m_PascalCase` for private members;
  - camelCase for locals;
  - `UPPER_CASE` for constants.
- **Comments** explain *why*, and the code keeps them short. Header comments open each major file with the
  module's purpose.
- **GUI tests** (`tests/EditorGuiTests.cpp`) find widgets by the exact text the editor prints and by screen
  position. Changing a label, status message, menu item or layout means updating those tests, and it
  changes what users see. The Controls panel (`SceneEditorScene::Controls()`) must fit its panel; a test
  checks this.
- **Pixel-checking tests** (render / shadow / shader / light tests) run the real pipeline. Keep
  floating-point operations and their order stable in math, physics and shader code unless the change is
  intended.
- **Changelogs:** every change set adds a numbered entry at the top of `CHANGES.md` (short) and a section at
  the top of `CHANGELOG.md` (how it works). User-facing editor features also belong in
  `docs/EditorTutorial.md` and `docs/Controls.md`.
