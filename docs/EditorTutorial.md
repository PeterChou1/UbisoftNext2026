# Scene editor tutorial

A walk through the basic actions of the **SceneEditor**. It takes about ten
minutes: you build a small level, give it behaviour with scripts, test it,
and play it in the Game.

- **Start the editor**
  - MacOS: `make run_editor` in `build/macos`.
  - Windows: run the `SceneEditor` project in Visual Studio.
- **Quit:** `Q` (ContestAPI), or close the window.

---

## 1. The screen

```
+---------------------------------------------------------------------------+
| [scene v]  New Revert Save Undo Redo   Play  Scene     Name [my_level] *  |
+-----------+-------------------------------------------------+-------------+
|[Pal][Hier]|                                                 | INSPECTOR   |
|  Select   |                                                 |  name       |
|  1 Rect   |            the field (your scene)               |  parent     |
|  2 Circle |                                                 |  position   |
|  ...      |                                                 |  size       |
|  6 Empty  |                                                 |  colour     |
|  BRUSH    |                                                 |  body / tag |
|  colours  |                                                 |  script     |
+-----------+-------------------------------------------------+-------------+
| status messages / key hints                                               |
+---------------------------------------------------------------------------+
```

- **Scene list** (top left): every scene file in `data/scenes`. Pick one to
  open it.
- **Left panel**, two tabs:
  - **Palette**: what you place. **Brush**: the settings new objects get.
  - **Hierarchy**: every object of the scene as a tree, children under
    their parent (section 5).
- **Field** (middle): your scene, seen by the game's 3D renderer.
  - Move the camera with **W A S D**, zoom with **Z / C**.
- **Inspector** (right): edit the selected object (**Object** tab) or the
  whole scene (**Scene** tab). The Object tab has two pages:
  - **Properties**: position, size, colour, body, tag and script;
  - **Components**: add, edit and remove components.
- **Name** (top right): the open scene's name. A `*` means unsaved changes.
- **Status bar** (bottom): what just happened and the key hints.

---

## 2. Start a new scene

1. Click **New**.

   A scene called `scene_1` (or `scene_2`, ...) is created, saved to
   `data/scenes/`, added to the scene list, and opened. It contains only the
   field.
2. Give it a real name: click the **Name** box at the top right, type
   `my_level`, and press **Enter**.

   The scene and its file are renamed. Spaces become `_`.

---

## 3. Place objects

There are three ways to place an object:

| Way | How |
|---|---|
| Click | Click a shape in the palette (or press **1**-**6**), then click the field. You keep placing that shape until you right click or press **Space** |
| Drag from the palette | Press a palette button, keep the mouse button down, drag onto the field and release where you want the object |
| Place and drag | While placing, press on the field and keep the button down: the new object follows the mouse until you release |

Set the **Brush** before placing:

- width / height, sides (polygons), rotation;
- body type, tag, model (for **5 Model**), and colour.

With **Snap (G)** on, positions snap to a 0.5 grid.

> Pressing on an existing object never stacks a new one on top of it: it
> grabs the object, so you can drag it even while a shape is selected in the
> palette.

---

### Your own 3D models (.obj)

1. Put the `.obj` file, and the `.mtl` file it uses, in `data/import/`.
2. Type its name (e.g. `pyramid`, the example that is already there) in the
   palette's **IMPORT .OBJ** box, and press **Enter** or **Import model**.
   You can also type a full path to an `.obj` anywhere on disk.

The model is copied into `data/models/` and selected in the brush, with the
**Model** tool active: click the field to place it.

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

**6 Empty** places an empty object: only a position and a rotation, drawn
in the editor as a light blue cross (nothing in the game). Use empties as:
- **groups**: the parent of other objects (section 5);
- **markers**: spawn points or waypoints;
- **holders**: for a script or components that belong to no shape.

## 4. Select and move objects

- **Select:** click an object. It gets a yellow outline and appears in the
  inspector. You can click any visible part of it, including the top of a
  tall object.
- **Move:**
  - Drag the object with the mouse. One drag is one undo step.
  - Or type the position in the inspector (section 6).
- **Rotate:** **R** turns it 15°, or type an angle in **Rot**.
- **Duplicate:** **F**. **Delete:** **X**.
- **Deselect:** right click.

The field can't be moved or deleted. Click it to change its colour, or its
size in the **Scene** tab.

---

## 5. The hierarchy: parents and children

Every object has a **Transform** (position, rotation, scale). A Transform
can have a parent: the child is then placed **relative to its parent**.
Moving, turning or scaling the parent carries its children along, and
deleting it deletes them too.

Open the **Hierarchy** tab at the top of the left panel:

```
[Palette][Hierarchy]
[     New Empty    ]
SCENE   6 objects
  Field
- Tank
    Hull
  - Turret
      Barrel
  Crate
```

| Action | How |
|---|---|
| Select an object | Click its row (selecting in the field also highlights its row, and unfolds its parents) |
| Make an object a child | Drag its row onto another row |
| Back to the top level | Drag its row onto **SCENE** |
| Fold / unfold children | The **-** / **+** in front of a parent |
| Add an empty | **New Empty**: under the selected object, or at the view's centre when nothing is selected |
| Scroll a long tree | **^** / **v** at the bottom |

The **Parent** box in the inspector (Properties) does the same by name:
type the parent's name and press **Enter**, or type `-` for the top level.

- **World positions:** a child keeps its place in the world when it gets a
  new parent. **Pos X / Pos Z** and **Rot** are always world values, so
  dragging a child in the field moves only that child.
- **Links:** in the field, the selected object shows lines to its parent
  (orange) and to its children (blue).
- **Refused:** an object can't go under one of its own children (that would
  be a loop). The field can't be a parent or a child.
- **Duplicate (F)** copies the object with all of its children. Every
  change of parent is one undo step.

> Example: in the `sandbox` scene, the empty `Orbit` has the `Rotator`
> script and two children, `Moon_1` and `Moon_2`. On **Play**, the script
> turns only `Orbit`, and the moons circle around it.

---

## 6. Type exact values

Every number in the inspector is an input box:

- Name
- Parent (an object's name, or `-`)
- Pos X / Pos Z
- Rot
- Width / Height / Size / Sides / Thick / Scale
- script parameters
- field size

To type a value:

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

---

## 7. Physics body and tag

In the inspector:

- **Body** decides how the object takes part in physics:

  | Body | Behaviour |
  |---|---|
  | None | Decoration, no physics |
  | Static | Solid, never moves (walls) |
  | Dynamic | Solid, pushed around (players, crates) |
  | Trigger | Not solid; reports touches to scripts (pickups, hazards) |

- **Tag** is the object's role, used by scripts: `Player`, `Pickup`, `Hazard`,
  `Wall`, and so on.

---

## 8. Give objects behaviour (scripts)

1. Select an object and use **Script < >** to choose a C++ script, e.g.
   `Rotator`, `Patrol` or `PlayerController`.
2. Its parameters appear below it (e.g. **Speed**). Type or step them.

For a script that runs the whole scene (rules, score, HUD):

1. Open the **Scene** tab.
2. Choose a **Script** such as `CollectGame` and set its parameters.

In the same tab you can:

- set the field size (**Field W / Field H**);
- press **Game camera = view** so the game starts with your current view.

The scene tab also shows whether the scene is **Playable**, or the first
problem (duplicate names, objects outside the field, unknown scripts).

> Example: a `Circle` with body **Dynamic**, tag **Player** and script
> `PlayerController`, a few `Polygon`s with body **Trigger**, tag **Pickup**
> and script `Collectible`, and scene script `CollectGame`. That is a
> complete collect-everything level.

### Components

Objects can also carry data components, such as `Health`, `Faction` and
`Waypoint`, or your own.

1. Select an object and click **Components** at the top of the inspector.
2. Choose a component with **Add < >** and click **Add**.
3. Its fields appear below its name. Edit them like any other value:
   - type numbers and text;
   - tick check boxes;
   - step enums with **< >**;
   - click colour swatches;
   - type another object's name to point at it.
4. Click a component's name to fold it. **Remove** takes the component off
   the object.

The picker also adds a physics body (**RigidBody**) or a **Script**, and
**Remove** takes them off again. Components are saved with the scene and
copied by **Duplicate**.

> Example: in the `sandbox` scene, select `Walker`. Its `Waypoint` points
> at `Waypoint_1`. Each marker's `Waypoint` points at the next one, so on
> **Play** the `WaypointFollower` script walks the loop.

To write your own components, whose fields and widgets come from a few
lines of C++, follow [ComponentsTutorial.md](ComponentsTutorial.md).

---

## 9. Test it

- Press **Play** (or **P**). Physics and scripts run inside the editor and
  the keyboard goes to the scripts, so WASD moves your player.
- Press **Stop** (or **P**) to end the test. The scene returns exactly to
  how it was before Play.

---

## 10. Save, open, revert

| Action | How |
|---|---|
| Save | **Save**. The `*` next to the name disappears |
| Open another scene | Pick it in the scene list. With unsaved changes, the first pick only warns you: **Save**, or pick it again to throw the changes away |
| Throw away changes | **Revert** reloads the scene from its file |
| Undo / redo | **Undo** / **Redo**, or **U** / **Y** |
| Save as plain text | **Scene** tab → **Plain text files**, then **Save** |

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

| Key | Action |
|---|---|
| W A S D | pan the camera |
| Z / C | zoom in / out |
| 1 - 6 | place Rectangle, Circle, Triangle, Polygon, Model, Empty |
| Space | back to Select |
| Right click | deselect / stop placing |
| R | rotate the selection 15° |
| F | duplicate the selection |
| X | delete the selection |
| U / Y | undo / redo |
| G | snap to grid on / off |
| P | play / stop |
| Tab | switch between the hardware and the software renderer |
| Enter / Esc | apply / cancel a typed value |

For how scenes and scripts work under the hood, see `CHANGELOG.md`. To
write your own components, see [ComponentsTutorial.md](ComponentsTutorial.md). For a
complete game made with the editor and scripts, see
`src/Game/Scripts/MetalInvasion/README.md`.
