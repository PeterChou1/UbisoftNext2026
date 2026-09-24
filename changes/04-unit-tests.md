# 04 – Unit tests

New folder: `tests/`.

| File | Purpose |
|---|---|
| `CMakeLists.txt` | Standalone test project (CMake ≥ 3.16). The root `CMakeLists.txt` can also include it with `-DBUILD_SERIALIZATION_TESTS=ON` (off by default, so the normal game build is unchanged) |
| `support/TestFramework.h` | Tiny dependency-free test framework (`TEST_CASE`, `CHECK`, `REQUIRE`, `CHECK_EQ`, `CHECK_THROWS_AS`). Nothing is downloaded |
| `support/GameWorldFixture.h` | Builds a realistic mid-game Metal Invasion world and captures an independent, field-by-field image of it for comparison |
| `support/TestCompat.h` | Force-included standard headers so the unchanged game sources also compile with GCC/libstdc++ (the game relies on MSVC/libc++ transitive includes). Only used by the tests |
| `support/stubs/app.h` | Headless stand-in for ContestAPI `app.h` (the engine files used by the tests include it without calling it), so no OpenGL/GLUT/SDL is needed |
| `TestMain.cpp` | Defines the global `ECS` instance and runs every test |
| `ArchiveTests.cpp` | Encoding, containers, corrupt-input handling, CRC |
| `ComponentSerializationTests.cpp` | Each component and resource on its own |
| `WorldSerializationTests.cpp` | Full game states end to end: robustness, compatibility, files |

## Running

```bash
# Standalone (any OS, no graphics libraries needed)
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
# or run directly, optionally with a name filter
./build/tests/SerializationTests            # all tests
./build/tests/SerializationTests "World:"   # only the world tests

# From the root project (needs CMake 4.0 like the game)
cmake -B build/tests-root -DBUILD_SERIALIZATION_TESTS=ON
cmake --build build/tests-root --target SerializationTests
```

On Windows/Visual Studio, open the generated solution and run the
`SerializationTests` project.

## The sample game state

`Fixture::BuildSampleLevel()` builds the kind of world the main level has in the
middle of **round 4's invasion phase**:

* ground, player base (damaged, static body, AI obstacle), unit selector
* a soldier battalion (moving, some selected, one killed), support miners
* a player tank and an enemy tank, each a root entity with two child meshes
  (parent/child transforms)
* enemy soldiers with individual walk speeds (one killed)
* crystal deposits with different amounts left
* an in-flight tank bullet and its particle emitter, an explosion growing, a
  laser beam, a particle
* a wall the player is currently placing (`GameState::ObstacleInCursor`,
  intersecting, red shader)
* round state (round 4, invasion, timers, crystals, difficulty, camera lerp)
* AI memory (tank targets, last known locations, line of sight, patrol
  targets, vector field configuration)
* the UI in build mode with a rotated wall (`UIState`)
* render handles set as if the renderer had already processed every entity

`Fixture::Capture()` copies every component, the entity allocator state,
`GameState` and the `BlackBoard` AI memory into a `WorldImage`.
`Fixture::Diff()` compares two images with explicit, **bit-exact**,
field-by-field comparisons and lists every difference. The tests therefore never
use the serializer to check the serializer.

The scene editor added 41 more tests (87 in total, 1050 checks); they are
described in [05-scene-editor.md](05-scene-editor.md).

## Test list (46 tests, 520 checks)

**Archive**
* primitives round trip; special float values (NaN, ±0, ±inf, denormals) are bit exact
* integers are little-endian with a fixed width
* strings and all supported containers round trip; hash maps serialize deterministically
* math types round trip bit exactly
* reading past the end throws, for every possible truncation point
* corrupt container length is rejected before allocating; invalid bool byte is rejected
* duplicate map keys are rejected; trailing data is detected
* the version is visible to `Serialize` functions; CRC-32 matches the standard check value

**Components and resources**
* `Transform` keeps hierarchy, TRS and matrices
* `RigidBody` keeps private mass/inertia/restitution; static and circular bodies
* physics scratch buffers are not persisted
* render handles are reset, and are not written at all
* every gameplay component round trips
* `GameState` keeps round flow, economy, difficulty and camera state
* `BlackBoard` keeps AI memory but not behaviour trees or grids
* `UIState` keeps the interaction mode (build mode, wall orientation) but not per-frame input

**World (complete game states)**
* a full game state survives save → clear → load
* **loading rewinds gameplay progress** made after the save (units moved and
  damaged, enemy killed, bullet exploded, crystal mined, reinforcement spawned,
  round advanced, AI memory changed)
* entity ids, parent/child links and cross references are preserved
* destroyed entities stay destroyed, and id allocation continues exactly as in
  the original session
* systems (visitors) see the restored entities; nothing is reported as deleted
* render components are marked for re-initialization
* save → load → save produces identical bytes
* empty world; entities without components; 4000-entity battle with holes

**Robustness (a bad file never changes the running game)**
* bit flips across the file, silent value tampering (caught by the checksum),
  truncation at many sizes, wrong magic, future format version, 200 random
  garbage buffers
* a component or resource layout mismatch (field added without a version bump)
  aborts the load before anything is applied

**Compatibility**
* unknown component types from a newer build are skipped with a warning
* layout versions are migrated (v1 save loads into a v2 build); a newer layout
  is refused by an older build
* registering a type or name twice is an error
* post-load callbacks run after the world is restored

**Files**
* save to disk and load back, including overwriting; no leftover `.tmp` file
* a missing file reports an error without touching the world
* two-phase load exposes the metadata before applying

## Making sure the tests can fail

A suite that always passes proves nothing, so bugs were injected on purpose
into the serializer and the suite was re-run each time. Every injected bug
was caught:

| Injected bug | Caught by |
|---|---|
| `GameState::LerpTime` not serialized | GameState resource test |
| `RigidBody::m_InvMass` not serialized | 8 tests (component + world) |
| Free-id queue order not restored | 9 tests (allocation, byte-identical re-save, world) |
| CRC check disabled | checksum tampering test |
| `Mesh::Loaded` not reset on load | render handle test |
| Hash maps written unsorted | determinism tests |
| `Transform::Children` not serialized | 7 tests (hierarchy, world) |
| Resources not applied | 9 tests |
| Resource layout check disabled | resource layout mismatch test |
| Component layout check disabled | component layout mismatch test |
| `UIState::flipped` not serialized | 7 tests (UIState resource, world, files) |

The first round of this exercise found two gaps: the render-handle test loaded
into default objects, and no test changed a value without breaking the file
structure. Both were fixed by the tests now in the suite.

## Verified configurations

* GCC 13 (Debug) with AddressSanitizer + UndefinedBehaviorSanitizer: all pass, no reports
  (46/46 at the save system commit, 87/87 with the scene editor)
* Clang 18 (Release): all pass
* Root project with CMake 4.4 and `-DBUILD_SERIALIZATION_TESTS=ON`, via `ctest`: pass
* Whole game (every `src/Game/*.cpp`) syntax-checked with Clang in the MacOS
  configuration (GLUT headers stubbed): no errors
