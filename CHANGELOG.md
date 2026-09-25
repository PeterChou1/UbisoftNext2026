# Changelog

A shorter, high level log of every change is in [CHANGES.md](CHANGES.md).

## Fragment shaders fixed, shadow maps, the light as a scene object

### Why the fragment shaders did not show

- `GameOptions::LineRendering` defaulted to `true`: the "hardware
  triangles" path (`RasterizerSystem::RenderLine`), which sends triangles
  to `App::DrawTriangle` and returns before `FragmentShaderSystem::Shade`.
  It lit shapes itself (a fixed Lambert term) and drew models in their
  flat material colour, so no fragment shader ever ran. The original game
  switched renderers in an options menu, which the engine split removed.
- **Now:**
  - `LineRendering` defaults to `false` (the software rasterizer) and
    `ShadowMapping` to `true`, in the editor and the game. Tab still
    switches.
  - `RenderLine` runs each triangle's own fragment shader once, on an
    8-wide `SIMDPixel` whose lanes hold the three corners (world position,
    normal, vertex colour, UV). The corner colours are sent to
    `DrawTriangle`, which blends them, so every shader shows in the fast
    path too (per-pixel effects like Stripes and shadows are approximate
    or absent there).
  - The test environment keeps the hardware path by default for speed;
    render tests switch to the software rasterizer.
- **Pulse** scaled the lit colour from 70% to 130%. Bright surfaces were
  clipped at white for most of the cycle, so nothing seemed to happen. It
  now goes from 55% to 100% plus a small white glow. **Rim** draws the
  body at 85% so its additive glow is visible on bright objects.

### Shadow maps

- **The pipeline:** with shadows on, `ClipperSystem` also projects every
  triangle with the light's matrix (`ShadowProjection`, computed by the
  vertex shaders), and `RasterizerSystem` draws them into the
  `DepthBuffer`'s shadow buffer. It keeps the largest 1 / w, i.e. the
  surface closest to the light.
- **The bug:** Blinn-Phong treated a pixel as shadowed only when
  `|closest - own| < 0.001` and `closest > own`, i.e. when the surface was
  almost the one in the shadow map. Surfaces behind an occluder (a large
  difference) were lit, so no shadow ever showed. Nothing turned
  `ShadowMapping` on either.
- **`ShadowSampling::Visibility`** (`src/Engine/ShadowSampling.h/.cpp`)
  projects each of the 8 pixels into the light. It returns lit when the
  pixel is behind the light or outside its view, and shadowed when
  `closest > own * (1 + 0.015)` (a relative bias against shadow acne,
  since 1 / w shrinks with distance). The Shape, Blinn-Phong and effect
  shaders multiply their diffuse term by it (`EffectShading::Lighting`
  takes the depth buffer and the flag).
- **When shadows are drawn:** `GameOptions::ShadowsOn()` =
  `ShadowMapping` (quality setting) && `LightShadows` (the light object's
  switch) && software rasterizer. The clipper, the rasterizer and the
  `ShaderHandler` use it.
- **Blinn-Phong:** the light's colour times its intensity. A zero
  material ambient (the default material of shapes) falls back to the
  light's ambient. `spec.pow(shininess)` discarded its result; now
  `spec = spec.pow(shininess)`.
- `DirectionalLight` gained `Intensity` and `Ambient`;
  `DepthBuffer::ToShadowSpace` is `const`.

### The light as a scene object (`src/Engine/World/SceneLight.h/.cpp`)

- **`SceneLight`** is a reflected component: Color, Intensity (shown as
  "Power"), Ambient, Pitch, Spread (the cone angle) and Shadows. It is
  registered for scene files and in the `ComponentCatalog`.
- **Settings:** the object's world position is where the light is (it is
  a spot light, so height matters), and its yaw is the direction along the
  ground. `Direction(yaw, pitch)` gives the beam, and `GroundTarget` where
  its centre meets y = 0. The defaults (position (0, 25, -5), pitch
  78.69°) are exactly the old fixed light.
- **`SceneLighting::Update`**, called by `GameManager::Update` every frame,
  applies the first light object (or the defaults) to the `Lighting`
  resource: position, target, cone, colour, intensity, ambient. It also
  sets `GameOptions::LightShadows`. `ScenePlayer` and the editor no longer
  place the light themselves.
- **Editor:**
  - `NewScene` also creates a "Directional Light"; prefab stages have
    none, and `CaptureStage` skips lights.
  - `LightObject`, `IsLight`, `AddLight` (above a point, shining down) and
    `AimLight` (yaw and pitch towards a point, one undo step).
  - Ground picking (`Pick`) never returns a light, so a light above an
    object never steals clicks from it.
- **GUI:**
  - The light gizmo (`DrawLightGizmo`): a ring with rays facing the view,
    the beam to the ground, and four cone edges.
  - The hierarchy row is orange, and the inspector labels it "Light".
  - Menus: **Create Light**, **Aim at View Center**, and **Aim Light Here**
    on objects.
  - `PickUnderMouse` first picks light and camera markers within 12 pixels
    on the screen. Along a nearly horizontal ray, one pixel covers about a
    world unit, more than a marker's pick radius.

## Shaders on meshes, editor camera controls, the game camera as an object

### Shaders tied to meshes

- **How an object is drawn:** every shape and model has a `FragShaderTag`
  and, optionally, a `VertShaderTag`. The `ShaderHandler` creates one shader
  instance per object in the `AssetServer`, and the `MeshHandler` writes
  the instance ids into the object's vertices. The renderer runs, for each
  vertex, `GetVertShader(v.VertexShaderID)` and, for each pixel,
  `GetFragShader(triangle id)`.
- **Fragment shaders** (`src/Engine/EffectShadersSIMD.h/.cpp`), 8 pixels at
  a time with the SIMD wrappers:
  - `EffectShading::BaseColor` takes the vertex colour (shapes) or, where it
    is black, the material's diffuse colour (models), so every effect works
    on both;
  - `EffectShading::Lighting` is the shape shader's ambient + Lambert term;
  - **Pulse** scales and whitens the lit colour with `sin(time)` (one value
    per draw, so the object pulses as a whole);
  - **Rim** adds a light tint where the surface turns away from the camera:
    `(1 - |N.V|)^3`;
  - **Stripes** brightens / dims bands of world height that scroll up:
    `fract(y * density - time * speed) < 0.5`.
  - `SIMD::Select` takes non-const references in the AVX2 build, so the
    shaders pass it copies.
- **Vertex shaders** (`src/Engine/EffectVertexShaders.h/.cpp`):
  - **Wave:** `y += A sin(t speed + (x + z) f)`.
  - **Sway:** x / z move by `strength * LocalPosition.Y * sin(...)`, so the
    base stays put and the top moves most.
  - Both compute the moved position in a local and project it like
    `DefaultVertexShader` (camera space, projection, light space for
    shadows). `v.Position` is **not** written: the vertex buffer keeps the
    world position, which the MeshHandler only rewrites when the object
    moves, so writing it would accumulate the offset every frame.
  - `DeltaTime` is the time the shader instance has run, advanced by the
    `ShaderHandler`.
- **Ids:** `PulseShaderID`, `RimShaderID`, `StripesShaderID`,
  `WaveVertShaderID` and `SwayVertShaderID` are appended to the enums in
  `Assets.h`. Scene files store the number, so existing values don't
  change. The `SERIALIZATION_ENUM_RANGE`s and `AssetServer::SetFragShader` /
  `SetVertShader` know the new ones.
- **MeshHandler fixes:** a changed vertex shader on a model called
  `UpdateMeshFragShader` (the vertex shader id ended up as the fragment
  shader id), and shapes never looked at vertex shader changes. Both now
  call `UpdateMeshVertShader`.
- **`ShaderLibrary`** (`src/Engine/ShaderLibrary.h`) lists the shaders the
  editor offers, with names and descriptions, plus `Name` / `Find`.
  `SceneObjects::SetFragmentShader` / `SetVertexShader` / `SetShaders` /
  `FragmentShaderOf` / `VertexShaderOf` add or change the tags.
- **Editor:** `SceneEditor::SetFragmentShader` / `SetVertexShader` (one
  undo step, shapes and models only), the inspector's **Shader** section,
  and `CopyObject` copies the shaders.
- **Prefabs:** `Prefab::Object` gained `FragShader` / `VertShader`.
  `FORMAT_VERSION` is 2: archives carry the file's version (`SetVersion`)
  and `Serialize(Object)` reads the two fields only when
  `ar.Version() >= 2`. Version 1 files give shapes the shape shader and
  models the lit one.

### The game camera is a scene object (`src/Engine/World/SceneCamera.h/.cpp`)

- **`GameCamera`** is a reflected component `{ Distance, Pitch, FieldOfView }`
  with ranges. It is registered in the scene file registry
  (`RegisterSceneSerializers`) and in the `ComponentCatalog` (by the
  catalog itself, so **Add Component** offers it and the inspector
  generates its fields).
- **The view** is the object's world position (the point looked at) and yaw
  (the direction along the ground, 0 = +Z), plus the component:

  ```
  eye = target + (back(yaw) * cos(pitch) + up * sin(pitch)) * distance
  back(yaw) = (-sin(yaw), 0, -cos(yaw))
  ```

  With yaw 0 and pitch 61.19° this is exactly the old fixed
  `VIEW_DIRECTION`, so existing scenes look the same.
- **`SceneCamera::`**
  - `Find` returns the first (lowest id) object with `GameCamera` +
    `Transform`;
  - `ViewOf`, `FromSettings` (the settings' camera), and `Current` (the
    camera object, else the settings);
  - `Apply` places the renderer's `Camera` and changes its field of view
    only when it differs;
  - `Create` and `SetView` (clamped to the component's ranges);
  - `Follower::Update` applies the current view only when it changed, so a
    script that drives the renderer's camera itself is not overridden
    every frame.
- **`ScenePlayer`** resets its follower when a world is restored and
  updates it every frame, so scripts can move the camera object.
  `MetalInvasion` starts its WASD camera from `SceneCamera::Current()` and
  pans relative to the camera's yaw.
- **Editor:**
  - `NewScene(withCamera = true)` creates a **Main Camera** (tag `Camera`)
    where the old camera was; prefab stages have none, and `CaptureStage`
    skips cameras.
  - `GameCameraObject`, `IsCamera`, `AddCamera`, `SetGameCamera(view)`
    (creates one when missing; also updates the settings for older
    readers) and `SetCameraView` are all one undo step.
  - **Picking:** a ray through a camera's eye marker picks it before
    anything else (at the eye's height, so dragging it works). A camera's
    target cross is only picked when no other object is there, so a camera
    at the field's centre never steals clicks.

### The editor's view and controls (`SceneEditorScene`)

- The view is a `SceneCamera::View` (`m_View`) applied with
  `SceneCamera::Apply` every frame. WASD pans along `Forward` / `Right` of
  its yaw, the arrow keys orbit and tilt (10..89°), E / V move the target up
  and down, Z / C zoom, and Home resets. **Q** stays ContestAPI's quit key.
- **Play** resets a `SceneCamera::Follower` and updates it each frame (the
  game camera, as scripts move it). **Stop** re-applies the editor view,
  which was never changed. The prefab stage saves and restores the scene's
  view.
- **Selection keys:** I / K call `SceneEditor::SetHeight` (±0.5, one undo
  step; `Move` keeps the height), J / L turn by ∓/±15°. The Transform
  section has a **Pos Y** row (`ID_FIELD_POS_Y`).
- **Camera gizmo** (`DrawCameraGizmo`): the target cross, a line to the eye,
  a pyramid sized by the field of view, and the name. Points behind the
  view are skipped. Camera rows are yellow in the hierarchy, and the
  inspector labels them "Game camera".
- **Menus:** **Create Camera** on the scene; **Align with View** and **View
  Through Camera** on a camera.
- **Controls panel** (`RenderControlsPanel`, data in `Controls()`): drawn
  over the scene view, where it also blocks clicks to the view. **H**, the
  status bar's **Controls** button, **Close** and **Esc** toggle it. The
  status bar's hints and message are fitted to the room left of the button.

## Unity-style editor, context menus, prefabs, responsive GUI

### Responsive text (`src/Engine/UIText.h/.cpp`)

- The UI is laid out in virtual units that ContestAPI stretches over the
  window, but GLUT draws text in pixels. A label is therefore smaller in a
  large window and larger in a small one. The old widgets assumed 10 units
  per character, so labels drifted off center and spilled out of their
  boxes.
- **`UIText` measures text in virtual units for the current window:**
  - `Width`, `CapHeight`;
  - `Fit` (the beginning plus `..`) and `FitTail` (the end, for a box
    being edited);
  - `CenterX`, `CenterY`.
- **Metrics:** `SetWindowSize` and `SetMeasure` are called by
  `EditorMain.cpp` / `GameMain.cpp`, with `WINDOW_WIDTH` / `HEIGHT` from
  ContestAPI's `main.h` and `glutBitmapWidth`. Tests use a built-in
  Helvetica 18 width table. **ContestAPI is not modified.**
- **Widgets** (`Widget.cpp`): Button, TextLabel, DropdownList (button and
  items), FillBar, CheckBox (new optional `labelWidth`) and TextField all
  center and fit their text.
- **Editor text:** the editor's `Text` helper cuts text to the panel it is
  drawn in (`EditorStyle::PanelRight`). Rows are vertically centered, and
  toolbar buttons are as wide as their label needs.

### Editor GUI rework (`src/Editor`)

- **Files:** the GUI is split into
  - `SceneEditorScene.cpp`: input, menus, actions, documents, prefab mode,
    toolbar, overlay;
  - `EditorLeftPanel.cpp`: hierarchy and assets;
  - `EditorInspector.cpp`;
  - `ContextMenu.h/.cpp`;
  - `EditorStyle.h`: layout, colours, widget ids, helpers.
- **`ContextMenu`:** items with actions or children.
  - Levels are sized from their labels and kept on the screen; submenus go
    right, or left at the edge.
  - Hovering opens a submenu in the same frame. Clicks outside, a right
    click or Esc close the menu.
  - The click that opened it is ignored, and while it is open the other
    widgets get no clicks.
- **Palette and brush removed.** `CreateItems(position, parent)` builds the
  create menu: Empty, the 4 shapes, Model > (every model), Prefab > (every
  prefab). `ObjectItems(e)` builds an object's menu.
  - Right click in the scene view: the object's menu (it is selected), or
    create at the clicked point.
  - Hierarchy: right click a row, right click the empty part, or the **+**
    button.
- **Assets:** `StartPlacingPrefab` / `StartPlacingModel`; click the scene
  to place (press and drag to move the new object), or drag the row onto
  the scene. A right click or Esc stops.
- **Inspector:** one scrolling list of rows (`Row`), with foldable
  `Section`s and a `Scrollbar` (drag the thumb, click the track to page).
  - The scroll resets when the selection changes.
  - Text boxes that are scrolled away drop their edit, so the keyboard is
    never left captured (`m_DrawnFields`).
  - **Add Component** opens a menu at the button. RigidBody and Script
    sections can be removed.
- **Prefab mode (`EditPrefab`, `NewPrefab`, `SavePrefab`, `BackToScene`):**
  - `SceneEditor::Suspend` / `Resume` keep the scene, its undo history,
    dirty flag, selection and camera.
  - `OpenPrefabStage` shows the prefab alone and unlinked.
  - Back to Scene warns once about unsaved changes, then calls
    `UpdatePrefabInstances`.

### Prefabs (`src/Engine/World/Prefab.h/.cpp`)

- **`Prefab::Data`:** objects in parent-first order. Each object has its
  name, tag, parent index, local pose, type (empty / shape / model), shape
  or model, body, script and parameters, and reflected components as bytes
  (by field name).
- **`Capture(root, name)`:**
  - The root keeps its rotation, scale and height; its X / Z become 0.
  - Entity fields become "object number + 1" inside the group, or 0 for
    anything outside it. They are rewritten temporarily on the live data
    and restored.
- **`Instantiate(prefab, position, yaw, parent)`** builds the objects, links
  them, sets the poses, then the bodies, scripts and components. It then
  remaps Entity fields to the new copies and adds a `PrefabLink` to the
  root.
- **Files:**
  - binary: `UBPF`, version, size, CRC32, payload;
  - text: `UBPF-TEXT 1`, then `prefab "name" count`, one `object ...` line
    per object, and `end`. Text follows `WorldSerializer::FileFormat()`.
- **`Load` checks:**
  - the magic, version, size and CRC;
  - parents come first;
  - numbers are finite, models are named, shape sizes are positive;
  - enums are in range;
  - component data parses (`ComponentEntry::ValidateBytes`).

  Unknown scripts or components are warnings, and unknown components are
  skipped when placing.
- **Also:** `Find` / `ClearCache` (used by `Script::SpawnPrefab`),
  `Available`, `PathOf`, `SafeName`.
- `ComponentCatalog` entries gained `SaveBytes` / `LoadBytes` /
  `ValidateBytes`. `PrefabLink` is a new scene component.
- **Editor core:**
  - `Create(kind, position, parent)`;
  - `PlacePrefab`, `PrefabOf`, `CapturePrefab`, `LinkPrefab`,
    `UnpackPrefab`;
  - `ResetToPrefab` (same place, yaw, parent and name; references cleared);
  - `UpdatePrefabInstances` (one undo step);
  - `CaptureStage` (several top level objects are grouped under a root);
  - `OpenPrefabStage`, `MarkSaved`, `Suspend` / `Resume`.
- **Sample:** `data/prefabs/turret.ubprefab`, written by `author_scenes`
  (new optional prefab directory argument) and checked by `SceneFileTests`.
  The `sandbox` scene has two instances.

### Tests

- **New files:**
  - `PrefabTests` (4): capture / place with layout, data and reference
    remapping; binary / text round trips; damaged files; `SpawnPrefab`;
  - `EditorPrefabTests` (5);
  - `UITextTests` (2).
- **GUI tests rewritten (21):**
  - context menus on the ground, on objects and in the hierarchy;
  - assets placing and dragging;
  - inspector sections, Add Component and Remove;
  - inspector scrolling;
  - the prefab editor;
  - window sizes from 640x480 to 2048x1536, checking that every text stays
    inside its panel, buttons are centered and menus stay on the screen.

## Scene hierarchy and empty transforms

### How the hierarchy fits the ECS

- The hierarchy is not a new system. It is the `Transform` component's
  `Parent` / `Children` fields, which the save files already stored.
  - Local values (`LocalPosition` / `LocalRotation` / `LocalScale`, and
    `Affine`) are relative to the parent.
  - The world pose applies every ancestor: scale, rotate, translate.
- Systems that need world space ask the Transform:
  - **Rendering:** `MeshHandler` builds and updates meshes with
    `GetWorldTransform()`. `UpdateChild` marks the whole branch dirty when a
    parent changes, so children are re-transformed in the same frame.
  - **Physics:** `RigidBody::SyncTransform` reads the world position and
    rotation. `ForwardTransform` writes back through `SetWorldPosition`.
  - **Picking and outlines:** `SceneObjects::Contains` / `WorldOutline` use
    the world frame.
  - **Scripts and the editor:** `SceneObjects::GetPosition / SetPosition /
    GetYaw / SetYaw` are world values.
- For a root Transform all of these return exactly the local values, so
  nothing changes for objects without a parent. The committed scenes still
  render and simulate identically.

### Engine (`Transform`, `SceneObjects`)

- **Transform math fixes:**
  - `GetWorldPosition` looked up the first parent on every step of its loop
    (it never reached the grandparents).
  - `GetWorldRotation` multiplied in the wrong order, with the same loop
    bug.
  - `GetWorldTransform` rotated the child's offset by the child's own
    rotation instead of the parent's.
  - `SetGlobalRotation` applied the rotation twice to children.

  All four now share one implementation (`ApplyAncestors`). It is bounded,
  so damaged data can't loop forever.
- **New Transform members:** `ParentPose()`, `SetWorldPosition()` and
  `SetLocalPose()`. `UpdateChild` skips children that no longer exist
  instead of asserting.
- **`SceneObjects`:**
  - `CreateEmpty` / `IsEmpty`: an object with only a Transform and a
    SceneObject.
  - `SetParent(child, parent)`: keeps the world pose, refuses loops and
    self-parenting, and updates both `Children` lists.
  - `GetParent`, `GetChildren`, `IsAncestor`.
  - `RepairHierarchy()` fixes dead parents, children that don't point back,
    duplicate entries and loops.
  - `Destroy` removes the object from its parent's children first. It still
    destroys the whole branch.
  - An empty is picked within `EMPTY_PICK_RADIUS` and has no outline.
- **Loading:** the scene registry runs `RepairHierarchy` after every load,
  with a log warning when it fixed something. A hand-edited file with a
  parent loop loads without hanging any system.

### Editor

- **Core:**
  - `ObjectKind::Empty` (`Place` never gives an empty a body), and
    `AddEmpty(position, parent)` as a single undo step.
  - `SetParent`, which refuses the field and loops, and records no undo
    step when nothing changes.
  - `ParentOf`, `ChildrenOf`, `RootObjects` (the field first).
  - `Duplicate` copies the whole branch (`DuplicateTree` / `CopyObject`),
    keeping relative positions and world scale; the copy stays under the
    same parent.
  - `Remove` deletes the branch and clears Entity fields pointing into it.
  - The Components tab's Transform row shows "in Parent" or "n children".
- **GUI:**
  - The left panel has **Palette / Hierarchy** tabs. Six palette kinds fit
    the room of five (22 px buttons).
  - The tree (`RenderHierarchy`, `HierarchyRows`) is a depth-first list
    with folding, selection highlight, reveal-on-select and scrolling. Rows
    are dragged onto rows or onto SCENE, with a drop hint in the status bar
    and error messages for refused moves.
  - Key **6** places empties.
  - The Properties tab has a **Parent** field and a children count. Empties
    show only a Scale row, which scales their children.
  - The overlay draws empties as crosses (`DrawCross`; the selected one is
    larger and in the accent colour) and links from the selection to its
    parent and children.
- **Sample scene:** `sandbox` has `Orbit` (an empty with a `Rotator`) and
  two child moons.

### Tests

- `tests/HierarchyTests.cpp` has 11 tests:
  - world math checked against the Affine matrix products;
  - SetParent keeping the world pose, and parents carrying their children;
  - loops refused, destroy and detach, repairs, and a looping text file;
  - children drawn, picked and simulated in world space;
  - empties;
  - editor parent / undo, branch duplicate and delete, empties picking,
    scaling and saving;
  - the sandbox orbit, whose render mesh is checked in the vertex buffer.
- Two GUI tests drive the tree with the mouse: select, drag to parent, the
  loop refused, fold, drop on SCENE, New Empty. They also cover key 6, the
  cross lines, and the Parent field.
- The test AppStub now records overlay lines.
- Mutation check: rotating offsets by the child's rotation again (the old
  bug) fails 4 tests.

## Components: reflection, generic serialization, editor add / remove

### Reflection (`src/Engine/Reflection/Reflection.h/.cpp`)

- A `REFLECT(Type) { Field("Name", &Type::Member)...; }` block describes a
  type's fields.
  - The macro specialises `Reflection::Describe<Type>`, which derives from
    `Builder<Type>`, and `Field` is a builder method.
  - `Reflection::TypeInfoOf<T>()` builds the description once, on first use,
    and checks it: identifier-like unique names, `Min <= Max`, and known
    enum values. It returns a `TypeInfo`, the fields in declaration order.
- **Each `FieldInfo` holds:**
  - its name, label and tooltip;
  - the editor's `FieldType`: Bool, Int, Float, String, Vec2, Vec3, Color,
    Enum or Entity;
  - the stored `ValueTag` (`b i u f d s v2 v3 e`);
  - range, step, hidden / read only, and enum options and range;
  - type-erased `GetValue` / `SetValue`.
- **Values travel as `FieldValue`**
  (`variant<bool, int64, double, string, Vec2, Vec3>`). `SetValue`:
  - converts between integer and floating point numbers;
  - rounds integers and clamps to the range and to the C++ type;
  - refuses enum values that don't exist, NaN, and unrelated types (a
    string into a number).
- **Unsupported member types** (64-bit integers, structs, containers) are a
  `static_assert` with an explanation. Hints that don't fit the member
  (`AsColor` on a float, `Options` on a non-enum) throw `std::logic_error`.
- **Enums** take their valid values from `SERIALIZATION_ENUM_RANGE` when it
  is declared, otherwise from `.Options({...})` as 0..n-1.

### Generic serialization, by field name

- `Serialization::Serialize(Archive&, T&)` is defined for every reflected T.
  It is found through the archive's namespace, so `Dispatch` picks it up
  for components, resources and nested uses alike.
- **Layout:** `[n]`, then for each field its name, its type tag and its
  value.
  - Binary: a u32 count; then per field a string, a u8 tag and the value.
  - Text: `Name:tag value` words, e.g.
    `[3] Current:f 100 Max:f 100 Invulnerable:b false`. This uses the new
    `TextOutputArchive::WriteWord`.
- **Loading** reads each saved field by its own tag, so a field the type no
  longer has is skipped whatever its type.
  - It then looks the field up by name and applies it with `SetValue`.
  - Missing fields keep their defaults, and values that no longer fit are
    ignored.
  - Unknown tags or malformed headers throw `SerializationError`.
- **Versions** still work (`Register<T>(name, version)`), but matching by
  name makes most layout changes free.

### Component catalog (`src/Engine/Reflection/ComponentCatalog.h/.cpp`)

- `ComponentCatalog::Get().Register<T>(name, description, version)` records
  type-erased `Has / Add / Remove / Copy / Data` for the editor.
- It also registers the component in the scene serialization registry. The
  new `Serialization::SceneSerializationRegistry()` gives mutable access; the
  const `GetSceneSerializationRegistry()` is unchanged.
- The ECS auto-registers the type when it is first added, so components stay
  ordinary ECS components: systems and scripts use `HasComponent<T>` /
  `GetComponent<T>` as for engine components.
- Registration is idempotent. A name clash, or one type under two names,
  throws.
- `RegisterGameScripts()` calls the game's `RegisterGameComponents()`, so the
  editor, the game and the tests share one list.

### Editor

- **Core (`SceneEditor`):**
  - `ComponentsOf(e)`: `ComponentView`s with the built-in components first,
    their summaries, and whether they can be removed.
  - `AddableComponents(e)`, `HasComponent`.
  - `AddComponent` / `RemoveComponent`: RigidBody maps to `SetBody`
    Static / None, Script to `SetScript`, anything else to the catalog.
  - `SetField` / `GetField` by component and field name.
- **Editing rules:**
  - Every add, remove or field change is one undo step. A field set to the
    value it already has (for example after clamping) records nothing.
  - Entity fields only accept objects.
  - `Duplicate` copies catalog components.
  - `Remove` clears Entity fields that point at the removed object.
- **GUI (`SceneEditorScene`):** the object inspector has **Properties** /
  **Components** tabs. The Components tab has an **Add < >** picker, and
  lists components as headers (click to fold) with **Remove** buttons.
  - Fields are drawn by `RenderField` from their `FieldInfo`:
    - `NumberRow` for numbers and each vector axis;
    - `CheckBox`, `TextField` for strings and object names;
    - an enum `Stepper`;
    - colour swatches;
    - dimmed text for read-only fields.
  - Hovering a field with a tooltip shows it in the status bar. Rows that
    don't fit end with "More below: fold components".
  - Component text fields have fixed ids 100..699, so dynamic widget ids now
    start at 1000. The picker resets when the selection changes.

### Game examples (`src/Game/Scripts/Components`)

- **`Health`**: range, step, labels.
- **`Faction`**: an enum with a `SERIALIZATION_ENUM_RANGE`, a string, a
  colour, and a read-only counter.
- **`Waypoint`**: an Entity reference with a tooltip.
- **Scripts:**
  - `WaypointFollower` walks the Waypoint chain.
  - `DamageZone` removes Health on contact and destroys the object at 0
    unless told not to.
- The `sandbox` sample scene has a waypoint loop with a walker, a damage
  zone, and a player with `Health` and `Faction`.

### Other changes

- **Scene files only contain the component types their entities use.**
  Before, every registered type was written, even with no instances, so
  registering a component changed every file.
  - Old files, with empty lists, still load.
  - The committed scenes were regenerated.
- **`Vec2::operator==`** returned false for equal vectors (the comparison was
  inverted). It now compares exactly, like `Vec3`. Nothing in the engine
  relied on it; the component round-trip tests found it.
- **`TextOutputArchive::FormatFloat`** writes whole numbers without an
  exponent (`100`, not `1e+02`). They still read back to the same bits.
- **Docs:**
  - `docs/ComponentsTutorial.md` (new);
  - a Components section in `docs/EditorTutorial.md` and the README.
- **Tests:**
  - `tests/ComponentTests.cpp` has 17 tests: descriptions and their errors,
    conversions and ranges, exact binary / text round trips, schema
    evolution in both formats, damaged data, the catalog, scene files
    including hand edits and old files, editor add / remove / fields /
    duplicate / delete / undo, save and load, the scripts, and the sandbox
    scene.
  - Two GUI tests drive the Components tab with the mouse and keyboard.

## Plain text save files and custom .obj import

### Plain text saves

- **`TextArchive.h`:** `TextOutputArchive` / `TextInputArchive` have the same
  interface as the binary archives, so every existing
  `Serialize(ar, value)` function reads and writes text unchanged.
  - Numbers use the shortest decimal that reads back to the exact bits.
  - `bool` is `true` / `false`; strings are quoted and escaped; containers are
    written as `[n]` followed by their elements.
- **`WorldSerializer`:**
  - `Save(ecs, meta, SaveFormat::Text)` and `SaveText(...)` write the world as
    text: a `UBSV-TEXT 1` header, `meta`, `entities` (id runs as `a..b`), one
    `component "Name" version [count]` header per type followed by
    `entity: values` lines, then `resource` records and `end`.
  - `Parse` / `Load` / `LoadFromFile` recognise text by its header and
    convert it with `TextToBinary`. That rebuilds exactly the binary file
    `Save` would write, then parses it, so both formats share one validation
    path.
  - Text has no checksum, so it can be edited by hand. Errors name the line
    and never touch the world. Unknown types from a newer build are skipped
    with a warning; newer layout versions are refused.
- **The registry** gives each type `SaveText` / `TextToBinary` functions,
  created from its `Serialize` function.
- **Program-wide option:** `WorldSerializer::SetFileFormat` / `FileFormat`
  (default binary) is used by `SaveToFile`, `GameManager::SaveGame` and the
  editor's `SaveSceneToBytes`. In the editor it is the **Plain text files**
  check box in the Scene tab, and the status bar says when an opened scene
  is a text file. Editor undo snapshots stay binary.
- **Metal Invasion scene:** `data/scenes/metal_invasion.ubsave` is committed
  as plain text (1.2 KB instead of 21 KB). `SampleScene::PlainText` marks a
  sample scene as text. `author_scenes` writes it as text, and
  `SceneFileTests` checks each committed file is byte-identical in its
  format.
- **Enum safety:** serialized enums declare their valid range with
  `SERIALIZATION_ENUM_RANGE(Type, First, Last)`, and the compiler refuses a
  serialized enum without one. Both archives refuse values outside the range.
  Before, a damaged file could hold any value, which was undefined behaviour;
  UBSan found this while testing hand-edited text.

### Custom .obj import

- **`Engine/ModelImport`:** `ModelImport::Import(path)` copies the model into
  `data/models/`.
  - It first checks that the file loads and has faces.
  - It copies the `.mtl` libraries the file uses. A different library with
    the same name is renamed, and the `mtllib` line is rewritten.
  - Model names are made safe. The same file imported again is reused; a
    different file with the same name gets `_2`, `_3`...
  - It accepts quoted paths and `~/`, and drops any cached copy of the model
    (`AssetServer::ForgetModel`).
- **OBJ reader (`Utils::LoadInstance`):**
  - faces with any number of corners are fan-triangulated;
  - `v`, `v/vt`, `v//vn` and `v/vt/vn` corners, with negative (relative)
    indices;
  - out-of-range or invalid indices skip the face instead of reading out of
    bounds;
  - a missing `.mtl` or unknown material uses the default material, with a
    log warning.

  Every model in `data/models` loads bit for bit as before (checked against a
  fingerprint of the old loader).
- **Editor:** the palette's **IMPORT .OBJ** box and **Import model** button
  take a path, or a file name in `data/import/`. After an import the model is
  selected in the brush with the Model tool active. `TextField` gained a
  maximum length (paths up to 260 characters).
- **Example:** `data/import/pyramid.obj` / `.mtl` (a quad base plus
  triangles), and `data/import/README.md`.

### Tests (143, clean under ASan / UBSan, Debug and Release)

- **`TextSaveTests`:**
  - byte-exact round trips of every scene feature and of every committed
    scene;
  - readability;
  - hand edits and CRLF line endings;
  - 11 kinds of mistakes, reported with their line and leaving the world
    untouched;
  - unknown and newer types;
  - escaped strings and special floats;
  - 300 randomly damaged files;
  - the file format option (files, `GameManager`, editor) and the editor
    check box;
  - enum ranges.
- **`ModelImportTests`:**
  - the OBJ reader (polygons, index forms, bad data, materials);
  - import copying and name clashes;
  - errors;
  - the editor's import box through the GUI, placing and rendering the
    imported model, and the example pyramid.

## Debug logging utility

`src/Engine/Log.h` / `Log.cpp`:

```cpp
LOG_INFO("Scene", "Loaded %s (%zu entities)", path.c_str(), count);
// [   12.345] INFO  Scene    | Loaded data/scenes/level_1.ubsave (13 entities)
```

- **Levels and categories:** `LOG_TRACE`, `LOG_INFO`, `LOG_WARN`,
  `LOG_ERROR`, each with a category and a printf format. GCC and Clang check
  the format against its arguments.
- **Debug only:** the macros print to standard output when `NDEBUG` is not
  defined (CMake Debug, Visual Studio Debug). In release builds they expand to
  nothing: the arguments are not evaluated and the strings are not in the
  binary. `-DENGINE_LOGGING=1` keeps logging in a release build;
  `-DENGINE_LOGGING=0` removes it from a debug build.
- **Windows:** `Game` and `SceneEditor` are GUI-subsystem programs without a
  console. The first message opens one, and every message is also sent to the
  debugger (`OutputDebugStringA`), so it shows in Visual Studio's Output
  window.
- **Thread safe:** the renderer's worker threads may log.
- **Runtime control:**
  - `Log::SetLevel(Log::Level::Warning)` drops the lower levels;
    `Log::Level::Off` silences everything.
  - `Log::SetSink(fn)` sends lines elsewhere (the tests use it).
- **What the engine logs:**
  - scene changes (`Scene`);
  - loads with their warnings, and saves (`Load` / `Save`, errors included);
  - scripts that are not registered or are the wrong kind (`Scripts`);
  - models that fail to load (`Assets`);
  - every editor status message (`Editor`).
- **Tests:** `tests/LogTests.cpp` covers formatting, levels, 8 threads, the
  engine's messages, and a translation unit compiled with `ENGINE_LOGGING=0`
  to check that disabled macros evaluate nothing. The test run is silent
  unless `UBI_TEST_LOG=1`. The suite passes in Debug and Release builds.



- **Typed values.** Every number in the inspector is a `TextField`: click,
  type, then **Enter** applies and **Esc** cancels; the first key replaces
  the old value. It covers:
  - name, Pos X / Pos Z, Rot;
  - Width / Height / Size / Sides / Thick / Scale;
  - script parameters, scene parameters, Field W / H.

  **- / +** still step the value.
  - Changes go through the same `SceneEditor` calls as before, so each is one
    undo step and positions stay on the field.
  - Invalid numbers are refused with a status message.
  - While a field has the focus, the editor ignores its keyboard shortcuts.
  - The first click outside a field only finishes the edit.
  - Object names must be unique (`SceneEditor::Rename`).
- **Scene documents.** The editor always has one open scene, named in the
  toolbar's **Name** box.
  - **New** creates `scene_<n>`, saves it immediately and adds it to the list.
  - Typing a new name renames the scene and its file.
  - Picking a scene in the list opens it. With unsaved changes the first pick
    only warns; picking again discards them.
  - **Revert** reloads the file.
  - The editor opens the first scene of the folder, or creates a new one.
  - `SetSceneDirectory` lets the tests use a temporary folder.
- **Dragging.**
  - Press a palette shape and drag it onto the field to drop it there.
  - While placing, press + drag places and positions in one gesture (one
    undo step).
  - Pressing an existing object grabs it, in either tool.
  - Picking uses the mouse ray through each object's height
    (`SceneEditor::PickRay`), so the visible top of tall or thick objects can
    be clicked.
  - Drags happen on the plane at the height the object was grabbed, so it
    stays under the cursor.
- **Text input plumbing.**
  - ContestAPI: `App::GetTypedText()`, a queue filled by the GLUT keyboard
    callback. It is needed because `App::Key` has no backspace, minus or
    period.
  - Engine: `Input::TypedText()` (sampled once per frame), and focus state in
    `UIState` (`focusedItem`, `editText`, `IsTyping()`).
  - The `TextField` widget with number / name filters.
- **Tutorial:** `docs/EditorTutorial.md`.
- **Tests:**
  - 6 new editor GUI tests: typed values, typing vs shortcuts, palette
    drag-and-drop, place-and-drag, tall-object picking, scene documents.
  - The scene list test was rewritten for the new document flow.
  - 122 tests in total, clean under ASan / UBSan.

## Metal Invasion rebuilt on the new engine (reference game)

The original game is back as `data/scenes/metal_invasion.ubsave` plus the
scripts in `src/Game/Scripts/MetalInvasion/`. It is a worked example of how to
build a game on the engine. It has the same rules as the original:

- rounds with preparation and invasion phases;
- crystals mined by support units;
- a shop in the base: soldiers, support, tanks and walls;
- battalion selection and move orders with path finding;
- enemy soldiers and tanks marching on the base, and game over.

[`src/Game/Scripts/MetalInvasion/README.md`](src/Game/Scripts/MetalInvasion/README.md)
maps every original system to its new place and explains which engine
feature each part uses.

Engine additions it needed, generic and usable by any script:

- **Mouse for scripts:** `MouseScreen()`, `MouseClicked()`,
  `MouseRightClicked()`, `MouseDown()`, and `MouseGround(point)` (the field
  point under the cursor).
- **`ScriptOf<T>(entity)`:** reach the script running on another object, e.g.
  to damage it.
- **Frame order:** `GameManager` now updates the mouse / click state before
  the scripts run, so scripts see this frame's clicks. Before, a click was
  set and cleared between two script updates.

Tests: `tests/MetalInvasionTests.cpp` has 13 tests playing the scene through
real frames. They cover rounds, the shop, selection and orders, path finding
around walls, mining, combat, tanks, walls, game over / restart, mouse input,
and saving mid-game. The suite (117 tests) passes, including under
AddressSanitizer / UndefinedBehaviorSanitizer.

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
| `MouseClicked`, `MouseRightClicked`, `MouseGround(p)` | `UIState` resource + `Camera` (the field point under the cursor) |
| `ScriptOf<T>(e)` | the script instance the `ScriptSystem` runs on entity `e` |
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
  UIStateManager         mouse position / clicks for the frame
  if scene simulates:    ScenePlayer always, SceneEditor only while playing
     PhysicsSystem       → contact events
     ParticleSystem
     ScriptSystem        sync → contacts → OnUpdate
  active Scene::Update   (editor GUI input, menu, Esc)
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

(Updated by "Editor usability" above; tutorial: `docs/EditorTutorial.md`.)

- **Palette:** Select, Rectangle, Circle, Triangle, Polygon and Model (keys 1-5).
  Click the field to place an object, or drag a palette shape onto it. The
  brush sets size, sides, rotation, body type, tag, model and colour.
  Snap-to-grid is toggled with G.
- **Viewport:** click to select; drag to move (one undo step per drag).
  Right click stops placing. WASD pans, Z / C zooms.
- **Object inspector:** typed or stepped name, position, rotation, size /
  height / sides / thickness or scale; colour, body (None, Static, Dynamic,
  Trigger), tag, model, script and its parameters. Buttons rotate (R),
  duplicate (F) and delete (X).
- **Scene tab:** scene script and its parameters, field size, and
  "Game camera = view". It also shows a live validation report.
- **Toolbar:** scene list (every file in `data/scenes`, picking one opens
  it), New (creates and saves `scene_<n>`), Revert, Save, Undo / Redo
  (U / Y), Play / Stop (P), and the scene's **Name** box (renames it).
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
| `EditorGuiTests` | the GUI through mouse / keys: palette placing and drag-and-drop, brush steppers, drag, typed inspector values, shortcuts vs typing, tall-object picking, scene tab, Play / Stop, New / rename / open / save / revert scenes |
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
