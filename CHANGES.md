# Changes (high level)

A short log of everything changed on this branch, newest first. Details and
explanations of how the systems work are in [CHANGELOG.md](CHANGELOG.md); the
editor tutorial is in [docs/EditorTutorial.md](docs/EditorTutorial.md), and
the components tutorial in [docs/ComponentsTutorial.md](docs/ComponentsTutorial.md).

## 10. Scene hierarchy tree and empty transforms

- **Hierarchy tab** (left panel, next to the Palette): every object as a
  tree of Transforms, children indented under their parent.
  - Click a row to select the object. Selecting in the field reveals its
    row: its parents unfold and the tree scrolls to it.
  - Drag a row onto another to make it a child, or onto **SCENE** for the
    top level.
  - Fold / unfold branches with **- / +**; the tree scrolls when long.
  - **New Empty** adds an empty under the selected object.
- **Empty transforms:**
  - **6 Empty** in the palette (key **6**) places an object that is only a
    Transform, for groups, markers, spawn points and script holders.
  - The editor draws it as a light blue cross, and nothing in the game.
  - It can be picked, dragged, scaled, parented, and given scripts and
    components.
- **Parent field** in the inspector: type a parent's name, or `-` for the
  top level. The viewport shows lines from the selection to its parent and
  children.
- **Children follow their parent:**
  - drawing, picking, outlines and physics bodies all use the world pose;
  - positions and yaws in the editor and in scripts are world values;
  - a child keeps its place in the world when its parent changes.
- **Duplicate and Delete** work on whole branches, and each change of
  parent is one undo step.
- **Engine fixes:**
  - The parent / child transform math was broken: the world position loop
    followed the first parent forever, and offsets were rotated by the
    child's rotation instead of the parent's.
  - `SetGlobalRotation` was wrong for children.
  - Scene files with broken or looping parent links are repaired on load.
- The `sandbox` scene has an example: an empty `Orbit` spun by the
  `Rotator` script carries two moons.
- 13 new tests (175 in total).

## 9. Components: add / remove in the editor, generated inspectors, saved by name

- **Reflection (`Engine/Reflection/Reflection.h`):** a component describes
  its fields once, with editor hints:

  ```cpp
  REFLECT(Health) { Field("Current", &Health::Current).Range(0, 10000).Step(10); ... }
  ```

  Supported: bool, integers, float, double, strings, Vec2, Vec3, enums,
  colours and object references. Hints: range, step, label, tooltip, read
  only, hidden.
- **Generic serialization, like Unity:** every reflected type is saved field
  by field (name, type tag, value) in binary and text, with no `Serialize`
  function. Loading matches fields by name:
  - added fields keep their default;
  - removed fields are skipped;
  - reordered fields and int / float changes keep their values;
  - values that no longer fit are ignored.
- **`ComponentCatalog`:** one `Register<T>("Name")` call puts a component in
  the editor and in scene files.
- **Editor:** the inspector's **Components** tab lists an object's
  components.
  - **Add** a registered component, a RigidBody or a Script; **Remove** takes
    one off.
  - Every field gets a generated widget: number box with - / +, check box,
    text, `< >` enum stepper, colour swatches, or object name.
  - Components fold away, and tooltips show in the status bar.
  - Every change is one undo step. Duplicate copies components, and deleting
    an object clears references to it.
  - `SceneEditor::AddComponent / RemoveComponent / SetField / GetField` do
    the same from code.
- **Examples** (`src/Game/Scripts/Components`):
  - components `Health`, `Faction` and `Waypoint`;
  - scripts `WaypointFollower` and `DamageZone`;
  - the `sandbox` scene uses them.
- **Tutorial:** [docs/ComponentsTutorial.md](docs/ComponentsTutorial.md).
- **Fixes:**
  - `Vec2 ==` returned false for equal vectors;
  - text saves wrote whole floats as `1e+02`, now `100`.
- **Scene files** only list the component types they use. Registering a new
  component no longer changes existing files, and older files still load.
- 19 new tests (162 in total).

## 8. Plain text save files, custom .obj import

- **Plain text saves (optional).** Scenes and saves can be written as readable
  text: one line per component record, strings quoted, exact numbers.
  - Toggle it with the editor's **Plain text files** option (Scene tab) or
    `WorldSerializer::SetFileFormat(SaveFormat::Text)`.
  - Loading detects the format. Text is converted to the binary form and
    checked by the same code.
  - Every existing scene converts to text and back byte for byte.
  - Enum values outside their range are now refused in both formats.
- **Custom model import.** Type a file name from `data/import/` (or any path)
  in the palette's **IMPORT .OBJ** box.
  - The `.obj` and its `.mtl` files are copied into `data/models/`, and the
    model is ready to place. Name clashes get numbered names.
  - The OBJ reader now accepts faces with any number of corners and negative
    indices. Bad indices are skipped instead of read out of bounds, and a
    missing `.mtl` falls back to the default material.
  - Existing models load exactly as before.
  - An example model, `data/import/pyramid.obj`, is included.
- `data/scenes/metal_invasion.ubsave` is now committed as a plain text scene.
  Sample scenes can be marked as text in `tests/scenes/SampleScenes.cpp`.
- 16 new tests (143 in total).

## 7. Debug logging

- `Engine/Log.h` provides `LOG_TRACE / LOG_INFO / LOG_WARN / LOG_ERROR(category, printf format, ...)`.
- It prints to standard output in debug builds, and compiles to nothing in
  release builds (`NDEBUG`). `ENGINE_LOGGING=0/1` overrides that.
- On Windows it opens a console for the GUI programs and also writes to the
  Visual Studio Output window.
- The engine logs scene changes, loads / saves, missing scripts, models that
  fail to load, and the editor's status messages.
- Tests stay quiet unless `UBI_TEST_LOG=1` is set.

## 6. Editor usability

- **Typed values:** the inspector's numbers are input boxes. Click a value,
  type, and press **Enter** (or **Esc** to cancel). This covers:
  - name, position (X / Z), rotation;
  - width / height / size / sides / thickness / scale;
  - script parameters, scene parameters, and the field size.

  The **- / +** steppers are still there. While you type, the editor's
  keyboard shortcuts are off.
- **Scenes:**
  - **New** creates a new scene (`scene_1`, `scene_2`, ...), saves it at once,
    and adds it to the scene list.
  - The toolbar's **Name** box renames the open scene and its file.
  - Picking a scene in the list opens it; with unsaved changes it asks for a
    second pick first.
  - **Load** became **Revert** (reload from the file).
  - The fixed `my_scene_1..3` slots are gone.
- **Dragging:**
  - Drag a shape from the palette straight onto the field.
  - While placing, press on the field and drag to place and position in one
    gesture.
  - Pressing an existing object always grabs it instead of stacking a new
    one on top.
  - Objects are picked where they are seen (the top of tall or thick objects
    too), and dragged at the height where they were grabbed.
- **Tutorial:** `docs/EditorTutorial.md` walks through the basic actions.
- **Engine and ContestAPI support for text input:**
  - `App::GetTypedText()` returns the typed characters (the key list had no
    backspace, minus or period).
  - `Input::TypedText()` and a reusable `TextField` widget (UI focus state in
    `UIState`).
  - `SceneEditor::Rename` and `SceneEditor::PickRay`.
- 122 tests, 14 of them driving the editor GUI with the mouse and keyboard.

## 5. Metal Invasion rebuilt as a reference game

- The original game is a scene file plus C++ scripts
  (`src/Game/Scripts/MetalInvasion/`), with no game code in the engine. It
  has the same rules: rounds, crystals, shop, battalions, path finding,
  enemy soldiers and tanks, walls, game over.
- New script helpers for any script:
  - mouse input (`MouseClicked`, `MouseGround`, ...);
  - `ScriptOf<T>(entity)` to reach another object's script.
- Scripts now see this frame's clicks: the UI input update runs before the
  scripts.

## 4. Windows build fix

- `NOMINMAX` is defined for every target, so `<windows.h>`'s `min` / `max`
  macros no longer break `std::min` / `std::max`.

## 3. Engine / SceneEditor / Game split, scripting, Metal Invasion logic removed

- The code is split into four targets:
  - **Engine** library: ECS, 3D CPU renderer, 2D physics, UI, serialization,
    scene objects, scripting;
  - **GameScripts** library;
  - **SceneEditor** program;
  - **Game** program.
- The original game code was removed; its engine pieces were kept and made
  generic.
- **Generic 2D scene objects:**
  - shapes: rectangle, circle, triangle, polygon, drawn by the 3D renderer as
    lit prisms;
  - 3D models by name;
  - physics bodies: none / static / dynamic / trigger;
  - tags.
- **C++ scripting on top of the ECS:**
  - object scripts and scene scripts, with parameters;
  - collision enter / exit callbacks;
  - the registry used by the editor.
- **Game:** a menu of the scenes in `data/scenes`, with sample levels
  (`level_1`, `level_2`, `sandbox`, `empty`).
- **Engine fixes:**
  - physics ran at a third of real time;
  - a debug-build crash on meshes without a material;
  - shape colours in the default renderer;
  - deleted objects still drawn;
  - picking of scaled objects;
  - model sizes normalised;
  - restarting could reload the wrong scene.
- **Build:** CMake targets `run` / `run_editor`, and headless tests with
  `-DBUILD_TESTS=ON`.

## 2. Scene authoring editor (first version)

- An in-game editor for placing prefabs and saving scenes to `data/scenes`.
  The committed scenes doubled as serialization tests. It was replaced by
  the generic SceneEditor in step 3.

## 1. Save system

- Binary, versioned, checksummed serialization of the whole ECS world:
  entities, components and resources. It supports:
  - two-phase loading, so a bad file never corrupts the running game;
  - migration of component layouts between versions;
  - atomic file writes.
- Headless unit tests.
