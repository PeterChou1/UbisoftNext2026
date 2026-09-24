# 02 – ECS and engine changes

These are small additions to existing engine files. They add what the save system
needs, and nothing existing behaves differently.

## `src/Game/EntityManager.h`

| Change | Why |
|---|---|
| `std::array<bool, MAX_ENTITIES> m_Alive` kept in sync by `CreateEntity`, `DestroyEntity`, `Clear` | The manager previously had no way to list living entities. An entity with no components has an empty signature, so signatures alone can't tell whether it is alive |
| `IsAlive(Entity)` | Liveness query |
| `GetLivingEntities()` | Living ids in ascending order, used when saving |
| `GetAvailableEntities()` | The free-id queue **in hand-out order** |
| `static ValidateState(living, available, error)` | Checks that every id in `[1, MAX_ENTITIES)` appears exactly once. The loader calls it before touching the ECS |
| `Restore(living, available)` | Rebuilds the allocator in an exact saved state |

## `src/Game/ECSManager.h`

| Addition | Why |
|---|---|
| `ClearWorld()` | Removes all entities, components and visitors **without** calling `ResetResource()` on resources (unlike `Reset()`) |
| `IsEntityAlive(Entity)` | Forwards to `EntityManager` |
| `GetLivingEntities()` / `GetAvailableEntities()` | Forward to `EntityManager` |
| `RestoreEntities(living, available)` | `ClearWorld()` + `EntityManager::Restore`, done before components are re-added |
| `HasResource<T>()` | Lets the save system skip resources a given ECS doesn't have (`GetResource` asserts) |
| `#include <typeinfo>` | `typeid` was used without this include |

Visitors (systems) need no special handling. `AddComponent` calls
`EntitySignatureChanged` as it always does, so every `ECS.Visit<...>()` sees
the restored entities. `VisitDeleted` sets are empty after a load, so systems
don't try to clean up entities that were "deleted" by the load.

## `src/Game/RigidBody.h`, `src/Game/Shape.h`

Added `friend struct SerializationAccess;`. `RigidBody` keeps its mass, inertia
and restitution in private members (`m_InvMass`, `m_InvInertia`,
`m_Restitution`). `Shape` keeps its shape type private (`m_ShapeEnum`). None of
these can be rebuilt from public data: `SetStatic()` zeroes the mass, and
`UpdateRadius` changes it. The friend gives only the serializer access, and the
public interface is unchanged.

## `src/Game/Scene.h`

New virtual hook, empty by default:

```cpp
virtual void OnWorldRestored() {}
```

It is called after a save replaced the world of a scene. Scenes use it to
rebuild runtime state that isn't stored in the file.
