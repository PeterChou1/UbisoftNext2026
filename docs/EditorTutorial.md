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
| PALETTE   |                                                 | INSPECTOR   |
|  Select   |                                                 |  name       |
|  1 Rect   |            the field (your scene)               |  position   |
|  2 Circle |                                                 |  size       |
|  ...      |                                                 |  colour     |
|  BRUSH    |                                                 |  body / tag |
|  colours  |                                                 |  script     |
+-----------+-------------------------------------------------+-------------+
| status messages / key hints                                               |
+---------------------------------------------------------------------------+
```

- **Scene list** (top left): every scene file in `data/scenes`. Pick one to
  open it.
- **Palette** (left): what you place. **Brush**: the settings new objects
  get.
- **Field** (middle): your scene, seen by the game's 3D renderer.
  - Move the camera with **W A S D**, zoom with **Z / C**.
- **Inspector** (right): edit the selected object (**Object** tab) or the
  whole scene (**Scene** tab).
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
| Click | Click a shape in the palette (or press **1**-**5**), then click the field. You keep placing that shape until you right click or press **Space** |
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

## 4. Select and move objects

- **Select:** click an object. It gets a yellow outline and appears in the
  inspector. You can click any visible part of it, including the top of a
  tall object.
- **Move:**
  - Drag the object with the mouse. One drag is one undo step.
  - Or type the position in the inspector (next section).
- **Rotate:** **R** turns it 15°, or type an angle in **Rot**.
- **Duplicate:** **F**. **Delete:** **X**.
- **Deselect:** right click.

The field can't be moved or deleted. Click it to change its colour, or its
size in the **Scene** tab.

---

## 5. Type exact values

Every number in the inspector is an input box:

- Name
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

## 6. Physics body and tag

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

## 7. Give objects behaviour (scripts)

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

---

## 8. Test it

- Press **Play** (or **P**). Physics and scripts run inside the editor and
  the keyboard goes to the scripts, so WASD moves your player.
- Press **Stop** (or **P**) to end the test. The scene returns exactly to
  how it was before Play.

---

## 9. Save, open, revert

| Action | How |
|---|---|
| Save | **Save**. The `*` next to the name disappears |
| Open another scene | Pick it in the scene list. With unsaved changes, the first pick only warns you: **Save**, or pick it again to throw the changes away |
| Throw away changes | **Revert** reloads the scene from its file |
| Undo / redo | **Undo** / **Redo**, or **U** / **Y** |

---

## 10. Play it in the Game

Run the **Game**. Its menu lists every scene in `data/scenes`, including
yours. Click it to play, and press **Esc** to go back to the menu.

---

## Shortcuts

| Key | Action |
|---|---|
| W A S D | pan the camera |
| Z / C | zoom in / out |
| 1 - 5 | place Rectangle, Circle, Triangle, Polygon, Model |
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

For how scenes and scripts work under the hood, see `CHANGELOG.md`. For a
complete game made with the editor and scripts, see
`src/Game/Scripts/MetalInvasion/README.md`.
