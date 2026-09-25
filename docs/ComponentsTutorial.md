# Components tutorial

This tutorial shows how to write your own component. You describe its fields
once in C++. From that description the scene editor builds the component's
inspector, and scene files save and load it. Nothing else is needed: no GUI
code and no `Serialize` function.

It takes about fifteen minutes. You write a `Health` component, add it to
objects in the editor, use it from a script, and then change its fields
without breaking scenes that are already saved.

- For the editor's basic actions, see [EditorTutorial.md](EditorTutorial.md).
- The finished examples are in `src/Game/Scripts/Components/`:
  - `GameComponents.h`: `Health`, `Faction` and `Waypoint`;
  - `ComponentScripts.*`: the `WaypointFollower` and `DamageZone` scripts;
  - the `sandbox` scene uses them.

---

## 1. Components and scripts

An object in a scene is an ECS entity, and components are the data attached
to it:

- **Built-in components:** `Transform`, `SceneObject` (name and tag),
  `Shape2D` or `Mesh`, `RigidBody`, and `ScriptComponent`.
- **Your components:** any plain struct, such as `Health { Current, Max }`.

Scripts hold behaviour; components hold data. A script reads and writes
components (`Get<Health>(entity)`), and the editor edits them. Keeping data in
components has three advantages:

- designers tune it per object in the editor;
- it is saved with the scene;
- any script can read it, not only the one that owns it.

---

## 2. Write the struct

Create a header in `src/Game/Scripts/Components/`. The build adds the files in
`src/Game/Scripts` automatically.

```cpp
#pragma once

#include "Reflection/Reflection.h"

struct Health
{
    float Current = 100.0f;
    float Max = 100.0f;
    bool Invulnerable = false;
};
```

The default values matter:

- a component added in the editor starts with them;
- a field missing from an older scene file keeps its default.

---

## 3. Describe its fields: `REFLECT`

Below the struct, at global scope:

```cpp
REFLECT(Health)
{
    Field("Current", &Health::Current).Range(0, 10000).Step(10);
    Field("Max", &Health::Max).Range(1, 10000).Step(10);
    Field("Invulnerable", &Health::Invulnerable).Label("Invuln.");
}
```

`Field("Name", &Type::Member)` declares one field:

- The **name** is what scene files store. It must be letters, digits and
  `_`, and unique within the component.
- The **member's C++ type** decides the widget.

Fields you don't declare are neither shown nor saved, which suits runtime
state such as timers or caches.

### Supported field types

| C++ type | Inspector widget | Saved as |
|---|---|---|
| `bool` | check box | `b` |
| `int`, `int8_t`, `int16_t`, `int32_t` | number box + `- / +` (whole numbers) | `i` |
| `uint8_t`, `uint16_t`, `uint32_t` | number box + `- / +` | `u` |
| `float` / `double` | number box + `- / +` | `f` / `d` |
| `std::string` | text box | `s` |
| `Vec2` / `Vec3` | one number row per axis (X, Y, Z) | `v2` / `v3` |
| `Vec3` + `.AsColor()` | colour swatches | `v3` |
| an `enum` | `< name >` stepper | `e` |
| `Entity` + `.AsEntity()` | text box: type another object's name | `u` |

64-bit integers, nested structs and containers are refused at compile time,
with a message saying so.

### Editor hints

Chain these after `Field(...)`:

| Hint | Effect |
|---|---|
| `.Range(min, max)`, `.Min(v)`, `.Max(v)` | Values are kept inside: typed values, `- / +` steps, **and loaded values** |
| `.Step(v)` | Increment of the `- / +` buttons (default 1 for integers, 0.1 otherwise) |
| `.Label("Text")` | Shown instead of the name (the file keeps the name) |
| `.Tooltip("Text")` | Shown in the status bar while the mouse is over the field |
| `.ReadOnly()` | Shown, not editable (e.g. a counter scripts update) |
| `.Hidden()` | Saved, but not shown in the inspector |
| `.Options({"A", "B"})` | Names of an enum's values |
| `.AsColor()` | A `Vec3` picked with colour swatches (kept in 0..1) |
| `.AsEntity()` | A `uint32` / `Entity` that points at another object |

Mistakes in the description are reported the first time it is used, with a
`std::logic_error` that names the field. Examples: two fields with the same
name, `Min > Max`, or `.AsColor()` on a float.

---

## 4. Register it

Add one line to `RegisterGameComponents()` in
`src/Game/Scripts/Components/GameComponents.cpp`:

```cpp
catalog.Register<Health>("Health", "Hit points");
```

This call does three things:

- it lists `Health` in the editor's **Add** picker;
- it adds `Health` to the scene file registry, so scenes save and load it;
- the ECS registers the type itself, the first time the component is added.

The string `"Health"` is written into scene files, so **never rename it** once
scenes use it. Registering the same type again does nothing. Using the same
name for two types, or two names for the same type, is an error.

Rebuild the **SceneEditor** and the **Game**. Both call
`RegisterGameScripts()`, which calls `RegisterGameComponents()`.

---

## 5. Use it in the editor

1. Select an object. The inspector lists its components as sections, like
   Unity: Transform, Shape2D or Mesh, RigidBody, Script, then the project's
   components.

   ```
   INSPECTOR
   Name   [Crate_1        ]
   Rectangle  #7
   Tag < - >
   - Transform
   - Shape2D     Rectangle
   - RigidBody   Static     [Remove]
   - Health                 [Remove]
       Current  [ 100.00 ] [-][+]
       Max      [ 100.00 ] [-][+]
       Invuln.  [ ]
   [        Add Component        ]
   ```
2. Click **Add Component** at the bottom and choose `Health` in the menu.
   The component appears with its default values.

   The menu offers:
   - **RigidBody** (adds a static body);
   - **Script** (adds the first object script);
   - every registered component the object doesn't have yet.
3. Edit the fields. They work like the other inspector values:
   - click a number and type it, then press **Enter**;
   - or use `- / +`.

   Each change is one undo step. Values outside the range are clamped, and
   values that don't fit the field are refused with a message.
4. To fold a component, click its title (`- Health`); click `+ Health` to
   unfold it. When the sections don't fit, a scrollbar appears on the right
   of the inspector.
5. To remove a component, click **Remove** on its row. **Undo** (U) brings it
   back.

`Transform`, `SceneObject`, `Shape2D` and `Mesh` define the object, so they
can't be removed. The field can't have components.

- **Duplicating** an object (F) copies its components.
- **Entity fields:** type the name of the object to point at, or `-` for
  none. When that object is deleted, the fields pointing at it are cleared.

---

## 6. Use it from a script

Scripts reach any component of any object:

```cpp
void DamageZone::OnCollisionEnter(Entity other)
{
    if (!Has<Health>(other))
        return;
    Health& health = Get<Health>(other);
    if (health.Invulnerable)
        return;
    health.Current = std::max(0.0f, health.Current - Param("Damage"));
    if (health.Current <= 0.0f)
        Destroy(other);
}
```

- `Has<T>(entity)` and `Get<T>(entity)` work for your components as well as
  the built-in ones.
- To add or remove a component while playing, use
  `ECS.AddComponent<Health>(e, Health{})` and `ECS.RemoveComponent<Health>(e)`.

The `sandbox` scene has a working example: a `Walker` with the
`WaypointFollower` script and a `Waypoint` component goes round three
`Waypoint_N` markers, and a `DamageZone` trigger takes `Health` from the
player.

---

## 7. How saving works (like Unity)

Each field is saved with its **name** and its **type tag**. A `Health` in a
plain text scene file (Scene tab → **Plain text files**) looks like this:

```
component "Health" 1 [2]
6: [3] Current:f 100 Max:f 100 Invulnerable:b false
9: [3] Current:f 35.5 Max:f 250 Invulnerable:b true
```

Binary files store the same thing: a count, then the name, a type byte and
the value of each field.

When loading, fields are **matched by name**, so you can change the struct
freely:

| You change | Scenes saved before load with... |
|---|---|
| Add a field | its default value |
| Remove a field | nothing: the saved value is skipped |
| Reorder fields | the same values |
| `int` ⇄ `float` ⇄ `double` | the value converted (rounded for integers) |
| `float Range` narrowed | the value clamped into the new range |
| A field's type to something unrelated (`string` → `float`) | its default value |
| An enum value that no longer exists | its default value |
| Rename a field | its default value (to the loader it's a new field) |

To rename a field without losing data, keep its saved name and change
only what the inspector shows: `Field("Current", ...).Label("HP")`.

You can also edit component records by hand in a text scene: any order,
missing fields, extra fields. Records that are broken, such as a value that
doesn't match its tag or an unknown tag, are refused with the line number,
and the scene in memory is left untouched.

A scene file only lists the component types it uses. Registering a new
component doesn't change existing files. A scene that uses a component the
program doesn't know is loaded without it, with a warning.

---

## 8. Enums

The editor needs the names of an enum's values, and loading needs to know
which values are valid. There are two ways to give them.

**`.Options`:** the values are 0, 1, 2 ...

```cpp
enum class Team { Neutral, Player, Enemy };

REFLECT(Faction)
{
    Field("Side", &Faction::Side).Options({"Neutral", "Player", "Enemy"});
}
```

**`SERIALIZATION_ENUM_RANGE`:** use it when the enum is also saved by a
`Serialize` function, or doesn't start at 0. Declare it **before** the
`REFLECT` block. `.Options` then names the values from the first one.

```cpp
SERIALIZATION_ENUM_RANGE(Team, Team::Neutral, Team::Enemy)
```

An enum without either is reported as an error. A value that doesn't exist is
refused, both in the editor and when loading.

---

## 9. Beyond components

`REFLECT` isn't limited to components. Any reflected struct can be
serialized:

- inside another `Serialize` function (`ar(myReflectedStruct)`);
- as a resource (`registry.RegisterResource<T>("Name")`);
- on its own (`Serialization::ToBytes(value)`).

Code can read the description too:

```cpp
const Reflection::TypeInfo& info = Reflection::TypeInfoOf<Health>();
for (const Reflection::FieldInfo& field : info.Fields)
    LOG_INFO("Health", "%s = %s", field.Name.c_str(),
             Reflection::ToString(field.GetValue(&health)).c_str());
```

The editor core edits fields by name the same way. The unit tests and tools
use this:

```cpp
editor.AddComponent(entity, "Health");
editor.SetField(entity, "Health", "Current", 40.0);   // one undo step
Reflection::FieldValue value;
editor.GetField(entity, "Health", "Current", value);
editor.RemoveComponent(entity, "Health");
```

A type is either reflected **or** has its own `Serialize` function, never
both. Hand-written `Serialize` functions (the built-in components use them)
remain the right choice for compact, positional layouts, and for containers.

---

## Limits and troubleshooting

- **Field types:**
  - up to 32-bit integers;
  - `float`, `double`, `bool`, `std::string`, `Vec2`, `Vec3`, enums;
  - no nested structs or containers.
- **Component types:** the ECS holds at most 32 component types
  (`MAX_COMPONENTS` in `Entity.h`), built-in ones included.
- **"Describe the type's fields with REFLECT(Type)":** the `REFLECT` block
  isn't visible where the type is used. Include the header that has it.
- **A component isn't in the Add picker:** it isn't registered, or the
  program wasn't rebuilt. Check `RegisterGameComponents()`.
- **A field is missing from the inspector:** it is `.Hidden()`, or the
  component is folded (click `+ Name`).
- **"More below: fold components":** the inspector is full. Fold the
  components you aren't editing.

Tests covering all of this are in `tests/ComponentTests.cpp` (reflection,
serialization, editor, scripts) and in
`tests/EditorGuiTests.cpp` (the inspector driven with the mouse and
keyboard).
