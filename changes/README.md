# Changes – Save system (serialization)

Branch: `claude/quirky-brahmagupta-skot0t`

This change adds a serialization system to the engine that saves and restores
the complete state of the game. It includes an in-game quick save / quick load
and a unit test suite that checks game states survive a round trip bit for bit.

## Documents

| File | Content |
|---|---|
| [00-repository-map.md](00-repository-map.md) | Map of the existing engine and game, written before any change |
| [01-serialization-system.md](01-serialization-system.md) | Architecture, file format, versioning, atomic loading, API |
| [02-ecs-and-engine-changes.md](02-ecs-and-engine-changes.md) | Additions to `EntityManager`, `ECSManager`, `RigidBody`, `Shape`, `Scene` |
| [03-game-integration.md](03-game-integration.md) | What is saved, what is rebuilt, AI refactor, `GameManager` API, controls |
| [04-unit-tests.md](04-unit-tests.md) | Test suite, sample game state, how to run, proof the tests can fail |

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
* **46 unit tests / 520 checks**, runnable without any graphics library. Also
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
README.MD                      save system feature + how to run the tests
```

## Known limitations

* The internal progress of AI nodes (for example weapon reload countdowns) is
  not saved. Units restart those timers after a load.
* `Camera`, `Lighting`, `GameOptions` and `UIState` are not part of a save. They
  are recreated by the scene's `Setup()`, so for example the camera goes back to
  its default position after a load.
* The full game could not be built and played in this environment (Linux, no
  GLUT/SDL; the game targets Windows and MacOS). Every game source file was
  compiled (syntax check) with Clang in the MacOS configuration against stubbed
  GLUT headers, and the changed files also with GCC, with no errors. The save system
  itself is covered by the unit tests. The in-game quick save/load path
  (`GameManager::LoadGame` → `MainLevel::OnWorldRestored`) should still get a
  manual play test on Windows or MacOS.
