# Scene editor controls

Every key and mouse action of the **SceneEditor**, grouped. The editor shows
the same list in its **Controls** panel: press **H**, or click **Controls**
at the bottom right. **Close**, **H** or **Esc** closes it.

The tutorial explains each feature in context: [EditorTutorial.md](EditorTutorial.md).

Keys do nothing while you type in a value box; they go to the box instead.
While the scene plays, the keyboard belongs to the scene's scripts, and only
**P** (stop) is the editor's.

## Scene view: the editor's camera

The scene view has its own camera. Moving it never changes the scene or the
game camera.

| Key | Action |
|---|---|
| W A S D | Pan along the ground, relative to where the view looks |
| Left / Right arrows | Orbit around the point the view looks at |
| Up / Down arrows | Tilt: look more straight down / more across |
| E / V | Move the view up / down |
| Z / C | Zoom in / out |
| Home | Reset the view |

**Q** is the framework's quit key, so it is not used for "down".

## Objects

| Key / mouse | Action |
|---|---|
| Click | Select (the field: deselect) |
| Drag | Move on the ground. The height is kept; one drag is one undo step |
| R or L | Rotate the selection +15° |
| J | Rotate the selection -15° |
| I / K | Raise / lower the selection by 0.5 (**Pos Y**) |
| F | Duplicate the selection, with its children |
| X | Delete the selection, with its children |
| Right click | Context menu: create an object here, or the object's actions |
| Hierarchy: drag a row | Parent it to another row (onto **SCENE**: top level) |
| Assets: click, then click the scene | Place a prefab or a model (again and again until right click / Esc) |
| Assets: drag a name onto the scene | Place it where it is released |

## Game camera (the Main Camera object)

The game camera is an object of the scene: what the game shows when the
scene plays.

| Action | How |
|---|---|
| Select it | Its hierarchy row (in yellow), its cross, or its eye marker in the scene view |
| Move / turn it | Like any object: drag, **Pos X / Y / Z**, **R / J**, **Rot** |
| Distance, pitch, field of view | Its **GameCamera** section in the inspector (**Dist.**, **Pitch**, **FOV**) |
| Make it show the current view | Right click it → **Align with View**, or **Scene** → **Game camera = view** |
| See what it sees | Right click it → **View Through Camera** |
| Add a camera | Right click the scene → **Create Camera** |
| Look through it | **P** (Play): the view switches to it until Stop |

## Editing

| Key | Action |
|---|---|
| U / Y | Undo / redo |
| G | Snap to the 0.5 grid on / off |
| P | Play / stop |
| H | Show / hide the Controls panel |
| Esc | Close a menu or the Controls panel, stop placing, cancel a typed value |
| Enter | Apply a typed value |
| Tab | Switch between the hardware and the software renderer |
| Q | Quit (ContestAPI) |

## Typed values

1. Click a value box. It lights up with a `_` cursor.
2. Type. The first key replaces the old value; **Backspace** deletes.
3. **Enter** (or a click elsewhere) applies; **Esc** cancels.

The **- / +** (or **< >**) buttons next to a box step the value instead.
