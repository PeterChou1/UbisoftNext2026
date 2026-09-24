# Changes – Save system (serialization) and scene editor

Branch: `claude/quirky-brahmagupta-skot0t`

This branch adds:

1. A serialization system that saves and restores the complete state of the
   game, with an in-game quick save / quick load.
2. A scene authoring GUI (title screen → **Scene Editor**) for building levels
   quickly. Scenes are saved with the save system to `data/scenes/`.
3. Five sample scenes committed in `data/scenes/`. Tests check them, and any
   scene you save from the editor, on every run.

Everything is covered by a headless unit test suite (87 tests), including a
harness that drives the editor GUI.

## Documents

| File | Content |
|---|---|
| [00-repository-map.md](00-repository-map.md) | Map of the existing engine and game, written before any change |
| [01-serialization-system.md](01-serialization-system.md) | Architecture, file format, versioning, atomic loading, API |
| [02-ecs-and-engine-changes.md](02-ecs-and-engine-changes.md) | Additions to `EntityManager`, `ECSManager`, `RigidBody`, `Shape`, `Scene` |
| [03-game-integration.md](03-game-integration.md) | What is saved, what is rebuilt, AI refactor, `GameManager` API, controls |
| [04-unit-tests.md](04-unit-tests.md) | Save system test suite, sample game state, how to run, proof the tests can fail |
| [05-scene-editor.md](05-scene-editor.md) | Scene editor: controls, architecture, committed scenes, engine fixes, editor tests |

## At a glance

* **Save everything that matters**: all living entities with their exact ids,
  16 component types, the `GameState`, `BlackBoard` and `UIState` (interaction
  mode) resources, and the entity allocator.
* **One `Serialize` function per type** handles both saving and loading, so the
  two can't drift apart.
* **Portable binary format**: little-endian, bit-exact floats, chunked,
  size-prefixed records, CRC-32 footer.
* **Versioned**: per-type layout versions with migration; unknown types from
  newer builds are skipped.
* **Atomic load**: the file is fully validated before the world is touched, so a
  bad save never corrupts the running game. The file itself is written through
  a temp file and a rename.
* **In game**: key **4** quick-saves and key **5** quick-loads
  (`saves/quicksave.ubsave`). AI behaviour trees and pathfinding grids are
  rebuilt after a load.
* **Scene editor**: palette, inspector, drag and drop, rotate, delete,
  undo/redo (serializer snapshots), grid snap, scene settings, validation,
  save/load to `data/scenes/`, **Play** to test in the main level and **Tab** to
  come back.
* **Committed scenes as serialization tests**: golden (byte-identical) checks
  for the samples, plus a load / validate / re-save check for every scene file
  in `data/scenes`.
* **87 unit tests / 1050 checks**, runnable without any graphics library,
  including a headless harness that clicks through the real editor GUI. Also
  clean under ASan/UBSan.

## Files

### New
```
src/Game/Serialization/Archive.h
src/Game/Serialization/Crc32.h
src/Game/Serialization/MathSerialization.h
src/Game/Serialization/EngineSerialization.h
src/Game/Serialization/SerializationRegistry.h
src/Game/Serialization/WorldSerializer.h
src/Game/Serialization/WorldSerializer.cpp
src/Game/Serialization/GameSerialization.h
src/Game/Serialization/GameSerialization.cpp
tests/CMakeLists.txt
tests/TestMain.cpp
tests/ArchiveTests.cpp
tests/ComponentSerializationTests.cpp
tests/WorldSerializationTests.cpp
tests/support/TestFramework.h
tests/support/GameWorldFixture.h
tests/support/TestCompat.h
tests/support/stubs/app.h
changes/*.md
.gitignore                     (build/, saves/)

src/Game/Prefabs.h/.cpp                     builders for every placeable object (game + editor)
src/Game/Editor/SceneEditor.h/.cpp          headless editor core
src/Game/Editor/SceneEditorScene.h/.cpp     editor GUI scene
data/scenes/*.ubsave                        5 authored sample scenes
tests/SceneEditorTests.cpp                  editor core tests
tests/SceneEditorGuiTests.cpp               GUI tests (headless harness)
tests/SceneFileTests.cpp                    committed scene file tests
tests/CameraPickingTests.cpp                screen <-> ground round trip
tests/scenes/SampleScenes.h/.cpp            authoring scripts of the sample scenes
tests/tools/AuthorScenes.cpp                writes the sample scenes (target author_scenes)
tests/support/AppStub.h/.cpp                scriptable headless App API
```

### Modified
```
CMakeLists.txt                 optional BUILD_SERIALIZATION_TESTS (OFF by default)
src/Game/EntityManager.h       living-entity tracking, allocator save/restore
src/Game/ECSManager.h          ClearWorld, RestoreEntities, HasResource, liveness queries
src/Game/RigidBody.h           friend SerializationAccess (private mass/inertia)
src/Game/Shape.h               friend SerializationAccess (private shape type)
src/Game/Scene.h               virtual OnWorldRestored()
src/Game/GameManager.h/.cpp    SaveGame / LoadGame / RequestSave / RequestLoad, status line
src/Game/MainLevel.h/.cpp      quick save/load keys, OnWorldRestored
src/Game/CreateMainLevel.h/.cpp RestoreMainLevelRuntimeState()
src/Game/PlayerUnits.h/.cpp    AI setup split into Attach*Behaviour functions
src/Game/BasicEnemyUnit.h/.cpp AI setup split, BasicEnemyUnit::Speed
README.MD                      save system + scene editor + how to run the tests

Scene editor:
src/Game/Scene.h               virtual SimulatesWorld() (editor freezes the world)
src/Game/GameManager.h/.cpp    skip simulation when frozen, BeginPlaytest / EndPlaytest
src/Game/GameTest.cpp          register the SceneEditor scene
src/Game/TitleScreenUI.cpp     "Scene Editor" button
src/Game/MainLevel.h/.cpp      Tab returns from a play test
src/Game/Widget.h/.cpp         DrawPanel widget
src/Game/Camera.cpp            exact mouse -> ground ray casting (bug fix)
src/Game/ShaderHandler.cpp     shader cleanup bug fixes
src/Game/PlayerUnits.cpp, BasicEnemyUnit.cpp, Crystal.cpp, CreateMainLevel.h/.cpp
                               spawn through Prefabs (behaviour unchanged)
tests/CMakeLists.txt           shared HeadlessGameCore library, AuthorScenes tool
tests/support/stubs/app.h      declares the App API subset (implemented by AppStub)
```

## Known limitations

* The internal progress of AI nodes (for example weapon reload countdowns) is
  not saved. Units restart those timers after a load.
* `Camera`, `Lighting`, `GameOptions` and the mouse/widget fields of `UIState`
  are not part of a save. They are recreated by the scene's `Setup()`, so for
  example the camera goes back to its default position after a load.
* **Behaviour change to check in game**: mouse → ground picking
  (`Camera::ScreenSpaceToWorldPoint`) was several world units off away from the
  screen centre and is now exact. Wall placement and unit move orders in the
  main level now land under the cursor. See
  [05-scene-editor.md](05-scene-editor.md).
* The full game could not be built and played in this environment (Linux, no
  GLUT/SDL; the game targets Windows and MacOS). Every game source file was
  compiled (syntax check) with Clang in the MacOS configuration against stubbed
  GLUT headers, and the changed files also with GCC, with no errors. The save system
  itself is covered by the unit tests. The in-game quick save/load path
  (`GameManager::LoadGame` → `MainLevel::OnWorldRestored`) should still get a
  manual play test on Windows or MacOS. The editor GUI was exercised through
  the headless harness. Its on-screen look and feel should be checked there
  too.
