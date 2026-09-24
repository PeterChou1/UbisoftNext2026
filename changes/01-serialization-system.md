# 01 – Serialization system (engine layer)

New folder: `src/Game/Serialization/`. It is picked up automatically by the
existing `GLOB_RECURSE` in the root `CMakeLists.txt`, so the game build needs no
changes.

| File | Purpose |
|---|---|
| `Archive.h` | `OutputArchive` / `InputArchive` binary archives + `Serialize` overloads for primitives, enums, `std::string`, `vector`, `array`, `pair`, `set`, `map`, `unordered_map` |
| `Crc32.h` | CRC-32 checksum used to detect corrupted/tampered saves |
| `MathSerialization.h` | `Vec2`, `Vec3`, `Vec4`, `Quat`, `Mat2`, `Mat3`, `Mat4` |
| `SerializationRegistry.h` | Maps each component/resource type to a **stable name** + **version** |
| `WorldSerializer.h/.cpp` | Saves/loads the entire ECS world: file format, validation, staged apply, atomic file IO |
| `EngineSerialization.h` | Engine components: `Transform`, `RigidBody` (+`Shape`, `AABB`), `Mesh`, `FragShaderTag`, `VertShaderTag`, `Particle`, `Emitter`, `UITarget`, `AIObstacle` |
| `GameSerialization.h/.cpp` | Game components, the `GameState` / `BlackBoard` / `UIState` resources, and the registry holding all of them |

## One function per type, for both save and load

Every type has a single function, found via argument-dependent lookup:

```cpp
template <typename Archive>
void Serialize(Archive& ar, CrystalDeposit& deposit)
{
    ar(deposit.AmountOfCrystal);
}
```

The same function runs with an `OutputArchive` when saving and an
`InputArchive` when loading. Because there is no separate "read" and "write"
code, the two can never get out of sync. That out-of-sync read/write code is the
most common bug in hand-written save systems.

## Encoding (portable between the Windows and MacOS builds)

* Integers are little-endian and always use `sizeof(T)` bytes.
* Floats are stored as their raw IEEE-754 bits, so a round trip is **bit exact**
  (NaN, ±0 and infinities included).
* `bool` is 1 byte and must be 0 or 1. Enums are stored as `int32`.
* Containers are stored as a `u32` count followed by the elements. On load the
  count is checked against the bytes left, so a corrupt length can never trigger
  a multi-GB allocation.
* Hash maps are written **sorted by key**, so the same state always produces the
  same bytes.
* Reading past the end throws `SerializationError`. It never reads garbage.

## Why types are identified by name, not `TypeID`

`TypeID<T>::VALUE` (in `TypeID.h`) is assigned during static initialization, so
the number a component gets can change between builds or platforms. Save files
therefore identify a type by the string given at registration:

```cpp
registry.RegisterComponent<Transform>("Transform");
registry.RegisterComponent<BasicEnemyUnit>("BasicEnemyUnit");
registry.RegisterResource<GameState>("GameState");
```

Components that are **not registered are not saved**. That is how purely
runtime data is kept out of the file, for example the `BehaviorTree` tag
component.

## Versioning

Each registration carries a layout version (default `1`). While loading,
`ar.Version()` returns the version stored in the file:

```cpp
template <typename Archive>
void Serialize(Archive& ar, Armor& a)
{
    ar(a.Health);
    if (ar.Version() >= 2)
        ar(a.ArmorPoints);          // field added in version 2
    else if constexpr (Archive::IsLoading)
        a.ArmorPoints = 10;         // default for old saves
}
registry.RegisterComponent<Armor>("Armor", 2);
```

* An old save loads into a new build: it is migrated by the code above.
* A new save loads into an old build: the load is **refused** with a clear error
  instead of guessing.
* A type the loader doesn't know (for example a component added in a newer
  build) is **skipped** with a warning. Every record is size-prefixed, which is
  what makes skipping possible.

## File format

```
Header  "UBSV" | u32 format version (1) | u32 flags
Chunks  u32 id | u64 size | payload           (repeated)
          META  key/value metadata (e.g. Scene = MainLevel)
          ENTS  MAX_ENTITIES | living entity ids | free entity queue (in order)
          COMP  per type: name, version, count, then per entity: id, u32 size, data
          RSRC  per resource: name, version, u32 size, data
          END   terminator
Footer  u32 CRC-32 of every preceding byte
```

## Loading is atomic

Loading happens in two phases:

1. **`Parse`** checks the CRC, magic, format version and chunk structure. It
   also checks that the entity allocator state is consistent (every id is either
   alive or free, exactly once) and that each component belongs to a living
   entity and appears at most once. Every component is deserialized into a
   staged action, and every resource is dry-run parsed. Every record must
   consume exactly its declared size, which catches a layout change made without
   a version bump. **The ECS is not touched during this phase.**
2. **`Apply`** clears the world and recreates the living entities with **their
   original ids**. It restores the free-id queue order, adds the components,
   writes the resources, then runs the post-load callbacks.

A corrupted, truncated, tampered or incompatible file is therefore rejected with
an error message, and the running game is left exactly as it was.

## Entity ids are preserved exactly

Components store entity ids that reference other entities: `Transform::Parent`
and `Children`, `GameState::ObstacleInCursor`, and the `BlackBoard` target maps.
Remapping every one of those would be error-prone. Instead, the save stores the
living ids and the **order of the free-id queue**. After a load the world has
the same ids, and `CreateEntity()` returns the same ids the original session
would have returned next.

## Runtime handles

`Mesh::Loaded`, `FragShaderTag::FragShaderID/Initialized`,
`VertShaderTag::VertShaderID/Initialized` and `Particle::loaded` point into
render caches that only exist for the current session. They are not written to
the file and are reset on load. `MeshHandler` and `ShaderHandler` then rebuild
the render data on the next frame, exactly as they do for a newly created
entity. `Shape::DebugPoints` and `Shape::ContactPoints` (per-step scratch
buffers) are also not saved.

## Files on disk

`WorldSerializer::SaveToFile` writes to `<path>.tmp` first and then renames it
over `<path>`, so a crash mid-save never destroys the previous save. Parent
directories are created as needed.

## Public API (summary)

```cpp
using namespace Serialization;
WorldSerializer serializer(GetGameSerializationRegistry());

std::vector<uint8_t> bytes = serializer.Save(ECS, {{"Scene", "MainLevel"}});
LoadResult r = serializer.Load(ECS, bytes);             // r.Success / r.Error / r.Warnings / r.Metadata

SaveResult s = serializer.SaveToFile(ECS, "saves/slot1.ubsave", metadata);
LoadResult l = serializer.LoadFromFile(ECS, "saves/slot1.ubsave");

// Two-phase load (read metadata, e.g. the scene, before changing anything)
WorldSnapshot snapshot;
LoadResult parsed = serializer.Parse(bytes, snapshot);
if (parsed) serializer.Apply(ECS, snapshot);
```
