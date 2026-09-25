# Contest Submission for Ubisoft 2026 (Programming category)

Top 5 contest submission for ubisoft 2026 next programming contest

[![](https://markdown-videos-api.jorgenkh.no/youtube/HWnY8v94AxY)](https://youtu.be/HWnY8v94AxY)


## Features

- Entity Component System
- 3D renderer multithreaded cpu based renderer
- Physics System (2D)
- Immediate mode gui library
- Save system: binary, versioned serialization of the whole world
- **SceneEditor**: a separate program to author scenes from basic 2D shapes (rectangle, circle, triangle,
  polygon), 3D models and empty transforms placed on a field and arranged in a parent / child hierarchy, with physics
  bodies, tags, components and C++ scripts
- **Game**: a separate program that plays the authored scenes, driven by C++ scripts
  (per object scripts and scene scripts, running on top of the ECS)
- **Prefabs**: groups of objects saved once and placed many times, edited on their own stage, instances updated
- **Reflected components**: describe a component's fields once (`REFLECT`). The editor then generates its inspector
  and lets you add and remove it, and scene files save it by field name, like Unity.

See [CHANGES.md](CHANGES.md) for a high level log of all changes and [CHANGELOG.md](CHANGELOG.md) for how the editor,
the scenes and the scripts work with the ECS.

## Project layout

| Directory | Target | What |
|---|---|---|
| `src/ContestAPI` | `ContestAPI` | window, input, drawing (unchanged) |
| `src/Engine` | `Engine` (library) | ECS, renderer, physics, UI, serialization, scene objects, scripting |
| `src/Game/Scripts` + `GameScripts.*` | `GameScripts` (library) | the C++ scripts and components (`Scripts/Components`), shared by the editor and the game |
| `src/Editor` | `SceneEditor` (program) | the scene editor |
| `src/Game` | `Game` (program) | scene menu + scene player |
| `data/scenes` | | authored scenes (`*.ubsave`) |
| `data/models` | | `.obj` / `.mtl` models usable in scenes |
| `data/prefabs` | | prefabs (`*.ubprefab`): reusable groups of objects |
| `tests` | `EngineTests`, `AuthorScenes` | headless unit tests and the sample scene generator |

## Dependencies

### Windows
* Visual Studio 2022 Community https://visualstudio.microsoft.com/vs/community/
    * This is a windows IDE, windows build system and c++ compiler all bundled together
* CMake (for build system) https://cmake.org/
    * This is a build system generator, which allows us to build the project on multiple platforms

### MacOS
* Homebrew (for package management) [get from homebrew website] https://brew.sh/
    * This is a package manager for macos systems
* CMake (see windows section)
* freeglut
    * This is an OpenGL utility library. Install by running [brew install freeglut] in a macos terminal
* SDL3
    * This is a cross-platform utility library. Install by running [brew install SDL3] in a macos terminal
* XCode command line tools
    * This has the MacOS build system and compiler. It should be installed automatically when installing homebrew, but can be installed through the MacOS terminal.

## To build

### Windows
* Run generate-windows.bat script in directory
* open visual studio solution in build/win64
* build and run from visual studio: `Game` is the startup project, set `SceneEditor` as startup project
  (right click > Set as Startup Project) to run the editor

### MacOS
* Run ./generate-macos.sh script in directory
* go to build/macos directory
    * run [make all] in macos terminal to build both programs
    * run [make run] to run the game
    * run [make run_editor] to run the scene editor
        * Note that the application can't be closed in the window. use cmd + Q to quit the program
* If using VSCode, commands can be run from the VSCode terminal

Both programs must run from the repository root (the build does this) to find `data/`.

## Scene editor

**New to the editor? Follow the tutorial: [docs/EditorTutorial.md](docs/EditorTutorial.md).**

* **New** creates a scene (added to the scene list and saved). Rename it by clicking the **Name** box (top right).
  Pick a scene in the list (top left) to open it, **Save** saves it, **Revert** reloads it.
* **Right click** the scene (or the hierarchy, or its **+**) to create objects: empties, shapes, models, prefabs.
  Right click an object for its menu: Create Child, Rename, Duplicate, Delete, Unparent, Focus, Save as Prefab, and
  Edit / Reset / Unpack for prefab instances.
* **Hierarchy** (left): the scene's objects as a tree of Transforms, children under their parent. Drag a row onto
  another to parent it, onto SCENE to make it top level; a parent carries its children when it moves / turns / scales.
  Empties (groups, markers, spawn points) are drawn as a cross.
* **Assets** (left, bottom): prefabs and models. Click one then click the scene (or drag it onto the scene) to place
  it. **New Prefab**, and the **import .obj** box.
* **Inspector** (right): the selected object's components as foldable sections (Transform, Shape2D / Mesh, RigidBody,
  Script, `Health` / `Faction` / `Waypoint` / your own), **Add Component**, **Remove**, and a scrollbar when they don't
  fit. Click any value and type it, **Enter** applies. See [docs/ComponentsTutorial.md](docs/ComponentsTutorial.md).
* **Prefabs:** Save as Prefab, or New Prefab, then edit it on its own stage (**Save Prefab**, **Back to Scene**): every
  instance in the scene is updated. Files in `data/prefabs/`; scripts spawn them with `SpawnPrefab("name", position)`.
* The **Scene** button shows the scene settings: scene script (e.g. `CollectGame`), field size, **Game camera = view**.
* **Two cameras:** the scene view has the editor's own camera (WASD pan, arrows orbit / tilt, E/V up / down, Z/C zoom,
  Home reset). The **game camera** is a scene object, **Main Camera**: select it in the hierarchy or the view, move and
  turn it like any object, edit its Distance / Pitch / FOV in its **GameCamera** section. Play looks through it.
* **Shaders:** each shape and model picks a fragment shader (Shape, Lit, Unlit, **Pulse**, **Rim**, **Stripes**, ...)
  and a vertex shader (Default, **Wave**, **Sway**) in its **Shader** section.
* Keys: WASD / arrows / E / V / Z / C / Home move the view, R L J rotate, I/K raise / lower, F duplicate, X delete,
  U/Y undo/redo, G snap, **P** play / stop, **H** controls panel, Esc, Tab renderer. All of them:
  [docs/Controls.md](docs/Controls.md) or the **Controls** button of the status bar.
* The GUI adapts to the window size: labels stay centered in their widgets and long text is shortened instead of
  spilling out of its panel.
* **Play** runs physics and scripts inside the editor, **Stop** restores the scene exactly.
* **Custom models:** drop an `.obj` (+ `.mtl`) in `data/import/`, type its name in the Assets' **import .obj** box and
  press Enter (or type a full path). It is copied to `data/models/` and ready to place. See
  [data/import/README.md](data/import/README.md).
* **Plain text scene files:** Scene settings → **Plain text files**. Saves become readable / diffable / hand-editable text;
  binary and text files both load everywhere.

## Game

The game lists the scenes of `data/scenes`: click one to play it. **Esc** goes back to the menu, **Tab** switches
between the hardware and the software renderer. `level_1` and `level_2` are small `CollectGame` levels (WASD / arrows:
collect every pickup, avoid hazards), `sandbox` shows every object type and the example components.

**`metal_invasion`** is the original Metal Invasion rebuilt as a scene + C++ scripts: the reference for how to make a
game with this engine. See [src/Game/Scripts/MetalInvasion/README.md](src/Game/Scripts/MetalInvasion/README.md).

## Scripts

Scripts are C++ classes in `src/Game/Scripts`, registered by name in `src/Game/GameScripts.cpp`:

```cpp
class Spinner : public Script
{
    void OnUpdate(float dt) override { SetYaw(Yaw() + Param("Speed") * dt); }
};
// RegisterGameScripts():
r.Register<Spinner>("Spinner", "Spins the object", {{"Speed", 90.0f, 15.0f}});
```

Derive from `SceneScript` for a script that drives a whole scene. Both programs pick new scripts up after a rebuild.

## Components

Data lives in components. Describe the fields once and register the component. The editor then offers it in
**Add Component** with a generated inspector section, and scenes save it by field name, so adding, removing or reordering fields
never breaks saved scenes:

```cpp
struct Health { float Current = 100; float Max = 100; bool Invulnerable = false; };

REFLECT(Health)
{
    Field("Current", &Health::Current).Range(0, 10000).Step(10);
    Field("Max", &Health::Max).Min(1);
    Field("Invulnerable", &Health::Invulnerable);
}
// RegisterGameComponents():
ComponentCatalog::Get().Register<Health>("Health", "Hit points");
```

Scripts use them with `Has<Health>(e)` / `Get<Health>(e)`. Full walkthrough: [docs/ComponentsTutorial.md](docs/ComponentsTutorial.md).

## Save files: binary or plain text

Scenes and saves are binary by default (compact, checksummed). Switch every file save to plain text with the editor's
**Plain text files** option or from code:

```cpp
Serialization::WorldSerializer::SetFileFormat(Serialization::SaveFormat::Text);
```

A text file lists each component record on one line, e.g.

```
UBSV-TEXT 1
meta [2] "Name" "level_1" "Scene" "Play"
entities 5000 [13] 1..13 [4986] 14..4999
component "SceneObject" 1 [13]
1: "Field" "Field"
6: "Circle" "Player"
...
resource "SceneSettings" 1
: "level_1" "CollectGame" [2] "Level" 1 "Lives" 3 24 18 0 0 -1 24
end
```

Loading recognises the format by itself, and text files get exactly the same validation as binary ones. Errors name the
line.

## Debug logging

```cpp
#include "Log.h"
LOG_INFO("MyGame", "Player picked up %s (%d points)", name.c_str(), points);
LOG_WARN / LOG_ERROR / LOG_TRACE(...)   // same form
```

In **debug** builds this prints to standard output, e.g.
`[   12.345] INFO  MyGame   | Player picked up Gem 3 (10 points)`.

- **Windows:** a console window opens with the program, and the lines also
  appear in Visual Studio's Output window.
- **Release builds:** the calls compile to nothing.
- **Runtime control:** `Log::SetLevel(Log::Level::Warning)` hides the
  chattier levels.

## Unit tests

The tests are headless (no OpenGL/GLUT/SDL needed) and run the real engine, editor and scripts on any platform:

```
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

They can also be built from the root project with `-DBUILD_TESTS=ON`. Set `UBI_TEST_LOG=1` to see the engine's debug log
while they run.
Regenerate the sample scenes in `data/scenes/` with `cmake --build build/tests --target author_scenes`
(`SceneFileTests` checks the committed files match `tests/scenes/SampleScenes.cpp`).
