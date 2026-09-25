# Scene editor tutorial

This tutorial walks through the **SceneEditor** in about fifteen minutes. You
build a small level, group objects, turn a group into a prefab, give objects
behaviour with scripts and components, test the level, and play it in the
Game.

The editor works like Unity's:

- a **Hierarchy** of the scene's objects;
- an **Inspector** of the selected object's components;
- right-click **context menus** to create things;
- **prefabs** you edit on their own stage;
- a **scene view** with its own camera, separate from the game's camera,
  which is an object of the scene.

Every key and mouse action is listed in [Controls.md](Controls.md), and in
the editor itself: press **H** or click **Controls** at the bottom right.

**Start the editor**

- MacOS: `make run_editor` in `build/macos`.
- Windows: run the `SceneEditor` project in Visual Studio.

**Quit:** `Q` (ContestAPI), or close the window.

---

## 1. The screen

```
+---------------------------------------------------------------------------+
| [scene v]  New Revert Save Undo Redo  Play  Scene   Name [my_level] *      |
+------------+--------------------------------------------+-----------------+
| HIERARCHY +|                                            | INSPECTOR      ||
| SCENE      |                                            | Name [Crate  ] ||
|   Field    |        the scene (your level)              | - Transform    ||
| - Tank     |                                            | - Shape2D      ||
|     Hull   |     right click: context menu              | - RigidBody    ||
|------------|                                            | - Health       ||
| ASSETS [New Prefab]                                     | [Add Component]||
| Prefabs    |                                            |                ||
|   turret   |                                            |                ||
| Models     |                                            |                ||
|   Box      |                                            |                ||
| [import  ][Import]                                      |                ||
+------------+--------------------------------------------+-----------------+
| status messages / tooltips / key hints                        [Controls]  |
+---------------------------------------------------------------------------+
```

- **Toolbar** (top):
  - the scene list (top left);
  - **New / Revert / Save / Undo / Redo**, **Play**;
  - **Scene** (scene settings in the inspector);
  - the scene's **Name**. A `*` means unsaved changes.
- **Hierarchy** (left, top): every object of the scene, as a tree. Children
  are indented under their parent.
- **Assets** (left, bottom): prefabs and 3D models to place, and the box to
  import `.obj` files.
- **Scene view** (middle): your level, drawn by the game's 3D renderer,
  seen through the editor's own camera (section 8):
  - **W A S D** pan, **Z / C** zoom;
  - the **arrow keys** orbit (left / right) and tilt (up / down);
  - **E / V** move the view up / down, **Home** resets it.
- **Inspector** (right): the selected object's components, one section each.
  A scrollbar appears when they don't fit.
- **Status bar** (bottom): what just happened, the tooltip of the value
  under the mouse, and the key hints. **Controls** (or **H**) opens the
  panel listing every control; **Close**, **H** or **Esc** closes it.

The layout adapts to the window: labels stay centered in their buttons, and
text that doesn't fit is shortened with `..` instead of spilling out of its
panel.

---

## 2. Start a new scene

1. Click **New**.

   A scene called `scene_1` (or `scene_2`, ...) is created, saved to
   `data/scenes/`, added to the scene list, and opened. It contains the
   field and the **Main Camera**, the camera the game plays with
   (section 8).
2. Give it a real name: click the **Name** box at the top right, type
   `my_level`, and press **Enter**.

   The scene and its file are renamed. Spaces become `_`.

---

## 3. Create objects: right click

**Right click** the scene where you want an object. The context menu
creates it right there:

```
Create Empty
Create Rectangle
Create Circle
Create Triangle
Create Polygon
Create Model   >   Box, GolfBall, ...
Create Prefab  >   turret, ...
Create Camera
```

- **Submenus:** hover an item with a `>` to open it.
- **Closing:** click an item to create it. A click elsewhere, another right
  click, or **Esc** closes the menu.
- **Snap:** with snap on (**G** toggles it), positions snap to a 0.5 grid.
- **Other ways to open the same menu:**
  - the **+** button next to **HIERARCHY** (creates at the centre of the
    view);
  - a right click on the empty part of the hierarchy.

New objects get default settings (1 x 1, grey, no physics body). Change them
in the inspector (section 6).

### Empties

**Create Empty** adds an object that is only a position and a rotation. The
editor draws it as a light blue cross; the game draws nothing. Use empties
as:

- **groups:** the parent of other objects (section 4);
- **markers:** spawn points or waypoints;
- **holders:** for a script or components that belong to no shape.

### Models and prefabs from Assets

- **Click then click:** click a name in **Assets**, then click the scene.
  Every click places one more, until you right click or press **Esc**.
- **Drag:** drag the name onto the scene and release it where you want it.
- **Place and drag:** while placing, press on the scene and keep the button
  down. The new object follows the mouse until you release.
- Right click an asset for **Place at View Center** (and **Edit Prefab**).

### Your own 3D models (.obj)

1. Put the `.obj` file, and the `.mtl` file it uses, in `data/import/`.
2. Type its name (e.g. `pyramid`, the example that is already there) in the
   **import .obj** box at the bottom of Assets, and press **Enter** or
   **Import**. You can also type a full path to an `.obj` anywhere on disk.

The model is copied into `data/models/`, listed under **Models**, and ready
to place: click the scene.

- **What the importer accepts:**
  - faces with any number of corners;
  - negative indices;
  - `.mtl` colours.

  A missing `.mtl` only gives a warning, and the model is drawn with the
  default material.
- **Size:** models are resized to a 1 unit footprint. Use **Scale** to size
  them.
- **Re-importing:** importing the same file twice reuses the first copy, and
  a different file with the same name gets a numbered name (`tree_2`).

---

## 4. The hierarchy: parents and children

Every object has a **Transform** (position, rotation, scale). A Transform can
have a parent: the child is then placed **relative to its parent**. Moving,
turning or scaling the parent carries its children along, and deleting it
deletes them too.

| Action | How |
|---|---|
| Select an object | Click its row. Selecting in the scene also highlights its row and unfolds its parents |
| Make an object a child | Drag its row onto another row |
| Create a child directly | Right click the parent (scene or hierarchy) → **Create Child >** |
| Back to the top level | Drag its row onto **SCENE**, or right click → **Unparent** |
| Fold / unfold children | The **-** / **+** in front of a parent |
| Scroll a long tree | The scrollbar on the right of the tree |

- **Parent box:** the **Parent** box of the inspector's Transform section
  does the same by name. Type the parent's name, or `-` for the top level.
- **World values:** a child keeps its place in the world when it gets a new
  parent. **Pos X / Y / Z** and **Rot** are world values, so dragging a child
  in the scene moves only that child.
- **Links in the scene:** the selected object shows lines to its parent
  (orange) and its children (blue).
- **Refused:** an object can't go under one of its own children, and the
  field can't be a parent or a child.

---

## 5. An object's context menu

Right click an object, in the scene or on its hierarchy row:

| Item | What it does |
|---|---|
| Create Child > | Creates an object (any kind, model or prefab) as its child |
| Rename | Starts typing a new name in the inspector (**Enter** applies) |
| Duplicate | Copies the object with all of its children (also **F**) |
| Delete | Deletes the object and its children (also **X**) |
| Unparent | Moves it to the top level, keeping its place |
| Focus | Centres the view on it |
| Save as Prefab | Saves it and its children as a prefab (section 7) |
| Edit / Reset to / Unpack Prefab | For prefab instances (section 7) |

- **Move:** drag the object with the mouse. One drag is one undo step.
- **Rotate:** **R** (or **L**) turns it 15°, **J** turns it back 15°.
- **Up / down:** **I** raises it 0.5, **K** lowers it (its **Pos Y**).
  Dragging keeps its height.
- **Deselect:** click the field.

A camera object's menu also has **Align with View** and **View Through
Camera** (section 8).

The field can't be moved or deleted. Click it to change its colour, or its
size in the **Scene** settings.

---

## 6. The inspector: components

The inspector shows the selected object as a list of **components**, like
Unity. Each component is a section:

| Section | Values |
|---|---|
| (top) | **Name**, kind and id, **Tag** (the object's role: `Player`, `Pickup`, `Wall`...) |
| Prefab | For instances: **Edit**, **Reset**, **Unpack** |
| Transform | **Parent**, **Pos X / Pos Y / Pos Z**, **Rot**; **Scale** for models and empties |
| Shape2D | **Width / Height** (or **Size**), **Sides** (polygons), **Thick**, colour |
| Mesh | The **Model** |
| Shader | The **Frag**ment and **Vert**ex shaders that draw a shape or a model |
| GameCamera | On cameras: **Dist.**, **Pitch**, **FOV** (section 8) |
| RigidBody | **Body**: Static, Dynamic or Trigger |
| Script | The **Script** and its parameters |
| Health, Faction, ... | The fields of the project's components ([ComponentsTutorial.md](ComponentsTutorial.md)) |

- **Fold / unfold** a section by clicking its title (`- Shape2D` /
  `+ Shape2D`).
- **Remove** takes a component off the object. Transform, Shape2D and Mesh
  define the object and stay.
- **Add Component** at the bottom opens a menu of what the object doesn't
  have yet: a **RigidBody**, a **Script**, or one of the project's
  components.
- **Duplicate** and **Delete** are below it.
- **Scrollbar:** when the sections don't fit, drag the scrollbar on the
  right, or click above or below its handle to move a page.

### Physics bodies

| Body | Behaviour |
|---|---|
| (no RigidBody) | Decoration, no physics |
| Static | Solid, never moves (walls) |
| Dynamic | Solid, pushed around (players, crates) |
| Trigger | Not solid; reports touches to scripts (pickups, hazards) |

### Type exact values

Every number is an input box: Name, Parent, Pos X / Y / Z, Rot, Width /
Height / Size / Sides / Thick / Scale, script parameters, component fields,
and the field size.

1. **Click** the value box. It lights up with a `_` cursor.
2. **Type** the new value. The first key replaces the old value, and
   **Backspace** deletes a character.
3. Press **Enter** to apply, or click anywhere else.
4. **Esc** cancels.

The **- / +** buttons next to each box step the value instead.

While you type, the editor's shortcuts are off: typing `x` into a name
doesn't delete the object. Positions are kept on the field, and a text that
isn't a number is refused, with a message in the status bar. Each change is
one undo step.

### Look: shaders

Every shape and model is drawn by two shaders, set in its **Shader**
section. The section's title shows them, e.g. `Rim + Wave`.

- **Frag < >** picks the fragment (pixel) shader, which colours the surface:

  | Shader | Look |
  |---|---|
  | Shape | Lit, in the shape's colour (the default for shapes) |
  | Lit | Blinn-Phong with the model's material (the default for models) |
  | Unlit | The flat material colour |
  | Pulse | Lit, its brightness pulses |
  | Rim | Lit, the edges facing away from the camera glow |
  | Stripes | Lit, bright and dim bands scroll upwards (hologram, scanner) |
  | Normals | Coloured by the direction of the surface |
  | Red | Solid red (highlights) |

- **Vert < >** picks the vertex shader, which can move the surface:

  | Shader | Motion |
  |---|---|
  | Default | None |
  | Wave | Bobs up and down in a wave travelling across the field |
  | Sway | Sways sideways, more at the top (grass, flags, beacons) |

The shaders animate in the scene view as well as in the game. Each change is
one undo step. Duplicates, prefabs and scene files keep them. The `sandbox`
scene has examples: a box with **Rim**, a golf ball on a **Wave**, and a
**Beacon** that sways with **Stripes**.

Scripts change them through the object's `FragShaderTag` / `VertShaderTag`,
or with `SceneObjects::SetFragmentShader(entity, RimShaderID)`.

### Behaviour: scripts

1. Select an object, click **Add Component**, and choose **Script**.
2. Pick the C++ script with **Script < >**, e.g. `Rotator`, `Patrol` or
   `PlayerController`.
3. Its parameters appear below it (e.g. **Speed**). Type or step them.

For a script that runs the whole scene (rules, score, HUD):

1. Click **Scene** in the toolbar.
2. Choose a **Script** such as `CollectGame` and set its parameters.

In the same place you can:

- set the field size (**Field W / Field H**);
- press **Game camera = view** to move the Main Camera to your current view;
- choose **Plain text files**;
- see whether the scene is **Playable**, or its first problem (duplicate
  names, objects outside the field, unknown scripts).

**Object** (the same toolbar button) goes back to the selected object.

> Example: a `Circle` with body **Dynamic**, tag **Player** and script
> `PlayerController`, a few `Polygon`s with body **Trigger**, tag **Pickup**
> and script `Collectible`, and scene script `CollectGame`. That is a
> complete collect-everything level.

---

## 7. Prefabs: reusable groups

A **prefab** is a group of objects (a root and its children) saved to
`data/prefabs/<name>.ubprefab`. You can place it any number of times. Each
copy is an **instance**, shown in blue in the hierarchy. Editing the prefab
updates every instance.

### Make one

**From objects in the scene**

1. Build the group, for example an empty `Tower` with a base and a turret as
   its children.
2. Right click the root → **Save as Prefab**.

   The prefab is saved under the object's name and appears in **Assets**.
   The object becomes its first instance.

**From scratch**

1. Click **New Prefab** in Assets.
2. The prefab editor opens with an empty root. Right click the root →
   **Create Child** to build the group.
3. Click **Save Prefab**, then **Back to Scene**.

### Place it

Click it in **Assets** and click the scene, drag it onto the scene, or right
click → **Create Prefab >**.

### Edit it: the prefab editor

Open it in any of these ways:

- right click an instance → **Edit Prefab**;
- right click the prefab in **Assets** → **Edit Prefab**;
- click **Edit** in the instance's Prefab section.

The scene is put aside, and the prefab is shown alone on an empty stage.

1. Edit it like a scene: create, move, parent, add components, Undo / Redo.
   The toolbar shows **PREFAB name**.
2. Click **Save Prefab** to write the file.
3. Click **Back to Scene**. The scene comes back as you left it (with its
   undo history), and **every instance of the prefab is updated**. That
   update is one undo step.

**Back to Scene** with unsaved prefab changes only warns you the first time.
Click it again to throw the changes away.

If the stage has several top level objects, they are grouped under a root
named after the prefab.

### Instances

- **Reset to Prefab:** replaces the instance by a fresh copy, at the same
  place, rotation and parent, with the same name.
- **Unpack Prefab:** turns the instance into ordinary objects, no longer
  updated by the prefab.

Scripts can spawn prefabs too: `SpawnPrefab("turret", position)`.

> Example: `data/prefabs/turret.ubprefab` is an empty root with a base, a
> spinning head and a barrel. The `sandbox` scene has two instances.

---

## 8. Cameras: your view and the game's

There are two cameras.

**The scene view's camera** is the editor's own. Moving it never changes the
scene.

| Key | Moves the view |
|---|---|
| W A S D | Pan along the ground, relative to where the view looks |
| Left / Right | Orbit around the point the view looks at |
| Up / Down | Tilt: look more straight down / more across |
| E / V | Up / down |
| Z / C | Zoom in / out |
| Home | Back to the default view |

(**Q** quits the program, so it is not used for "down".)

**The game camera** is an object of the scene: **Main Camera**, drawn in yellow
in the hierarchy. It is what the game shows when the scene plays. In the
scene view it is a cross at the point it looks at, a line up to its eye, and
a small pyramid at the eye pointing where it looks, with its name.

- **Select it:** its hierarchy row, its cross, or its eye marker (clicking
  the eye picks the camera before anything else; its cross never hides an
  object on the same spot).
- **Move / turn it** like any object: drag it, type **Pos X / Y / Z**, turn it
  with **R / J** or **Rot**. The position is the point it looks at, and
  **Rot** the direction it looks along the ground.
- **Its GameCamera section:**

  | Value | Meaning |
  |---|---|
  | Dist. | How far the eye is from the point it looks at |
  | Pitch | How steeply it looks down: 90 is straight down |
  | FOV | Field of view, in degrees |

- **Right click it:**
  - **Align with View** moves it to show exactly the scene view;
  - **View Through Camera** moves the scene view to what it sees.
- **More cameras:** right click the scene → **Create Camera**. The game uses
  first camera created (the lowest id).
- It can be a child of another object, and scripts can move it: the game
  follows it.

Scenes made before cameras were objects have none. They play with the camera
in their scene settings, and **Game camera = view** adds a Main Camera.

---

## 9. Test it

- Press **Play** (or **P**). Physics and scripts run inside the editor and
  the keyboard goes to the scripts, so WASD moves your player.
- The view switches to the **game camera**, so you see what the player
  will see.
- Press **Stop** (or **P**) to end the test. The scene returns exactly to
  how it was before Play, and the view to where it was.

---

## 10. Save, open, revert

| Action | How |
|---|---|
| Save | **Save**. The `*` next to the name disappears |
| Open another scene | Pick it in the scene list. With unsaved changes, the first pick only warns you: **Save**, or pick it again to throw the changes away |
| Throw away changes | **Revert** reloads the scene from its file |
| Undo / redo | **Undo** / **Redo**, or **U** / **Y** |
| Save as plain text | **Scene** → **Plain text files**, then **Save** |

Scene files are binary by default. With **Plain text files** checked, every
save is written as readable text instead. You can open it in any text editor,
compare it in version control, or edit values by hand. Both kinds open the
same way: the editor and the Game recognise the format by themselves.

---

## 11. Play it in the Game

Run the **Game**. Its menu lists every scene in `data/scenes`, including
yours. Click it to play, and press **Esc** to go back to the menu.

---

## Shortcuts

The full list, grouped, is in [Controls.md](Controls.md) and in the editor's
**Controls** panel (**H**). The most used:

| Key / mouse | Action |
|---|---|
| Right click | Context menu: create here, or the object's actions. While placing an asset: stop |
| W A S D | Pan the view |
| Arrow keys | Orbit (left / right), tilt (up / down) |
| E / V | View up / down |
| Z / C | Zoom in / out |
| Home | Reset the view |
| R or L / J | Rotate the selection +15° / -15° |
| I / K | Raise / lower the selection |
| F | Duplicate the selection (with its children) |
| X | Delete the selection (with its children) |
| U / Y | Undo / redo |
| G | Snap to grid on / off |
| P | Play / stop |
| H | Controls panel |
| Esc | Close a menu or the Controls panel, stop placing, cancel a typed value |
| Enter | Apply a typed value |
| Tab | Switch between the hardware and the software renderer |

For how scenes and scripts work under the hood, see `CHANGELOG.md`. To write
your own components, see [ComponentsTutorial.md](ComponentsTutorial.md). For
a complete game made with the editor and scripts, see
`src/Game/Scripts/MetalInvasion/README.md`.
