# 00 – Repository map (before the change)

This maps the repository as found at commit `e411a7a`, the starting point for
the save system.

## Top level

```
CMakeLists.txt            Build (CMake >= 4.0): ContestAPI library + Game executable (Windows / MacOS)
generate-*.sh/.bat        Project generation helpers, TC-*.cmake toolchains
data/                     .obj/.mtl models, sounds, controller mappings
src/ContestAPI/           Ubisoft NEXT framework: window, main loop, input, sound, sprites (GLUT, SDL3 on Mac)
src/Game/                 Everything written for the contest (~15k lines, flat folder)
*.pdf                     Contest API instructions and game documentation
```

## Frame flow

```
main (ContestAPI) ─┬─ Init()      GameTest.cpp: ECS.Init(), GameManager.Setup(), register scenes, SetActiveScene("TitleScreen")
                   ├─ Update(dt)  GameManager::Update → Physics → Particles → UI → Scene::Update → Shaders → Meshes → BlackBoard → AI
                   ├─ Render()    GameManager::Render → Vertex shader → Clipper → Rasterizer → Fragment shader → Scene::Render; ECS.FlushECS()
                   └─ Shutdown()
```

Two globals connect everything: `ECSManager ECS` and `GameManager GameSceneManager`
(both defined in `GameTest.cpp`, reached through `extern` elsewhere).

## Engine layers in `src/Game`

| Layer | Main files | Notes |
|---|---|---|
| **ECS** | `ECSManager.h`, `EntityManager.h`, `ComponentManager.h`, `ComponentBuffer.h`, `VisitorManager.h`, `Visitor.h`, `TypeID.h`, `Entity.h`, `Resource.h` | `Entity` = `uint32` id (0 = null, max 5000). Components are stored densely per type in `ComponentBuffer<T>`. A `Signature` bitset (32 types) records which components an entity has. **Visitors** are cached entity sets per component combination (`ECS.Visit<A, B>()`) and also track deleted entities per frame. **Resources** are global singletons (`ECS.GetResource<T>()`) with `ResetResource()`. `TypeID<T>` numbers are assigned during static initialization |
| **Scenes** | `Scene.h`, `GameManager.*`, `TitleScreen.*`, `MainLevel.*`, `WinScreen.*` | `SetActiveScene` = `ECS.Reset()` + `Scene::Setup()` |
| **Math** | `Vec2/3/4`, `Mat2/3/4`, `Quat`, `Transform`, `Utils` | `Transform` holds a parent/children hierarchy by entity id, a cached affine matrix and its inverse, and a 2D slice plane for physics |
| **Physics (2D)** | `RigidBody.*`, `Shape.h`, `AABB.h`, `SAT.*`, `Collision.*`, `Manifolds.*`, `PhysicsSystem.*`, `ColliderCallbackSystem.*`, `ColliderCategory.h` | Rigid bodies keep mass/inertia/restitution private. Collision callbacks are registered per category pair |
| **Renderer (CPU, multithreaded, SIMD)** | `VertexShaderSystem`, `ClipperSystem`, `RasterizerSystem`, `FragmentShaderSystem`, `*ShaderSIMD`, `MeshHandler`, `ShaderHandler`, `AssetServer`, buffers (`VertexBuffer`, `IndexBuffer`, `DepthBuffer`, `PixelBuffer`, `ColorBuffer`, `Tiles`), `Camera`, `Lighting` | `Mesh`/`FragShaderTag`/`VertShaderTag` components hold an asset id plus runtime handles into render caches |
| **AI** | `BehaviorTree.*`, `AINodes.*`, `BlackBoard.*`, `BlackBoardSystem.*`, `AISystem.*`, `Map.*` (vector field pathfinding) | Behaviour trees are polymorphic node graphs stored in `BlackBoard::BehaviorTreeDataBase`. An empty `BehaviorTree` tag component marks the entities that have one. `AIObstacle` entities block vector-field cells |
| **Particles** | `ParticleSystem.*`, `Emitter.h` | `Emitter` spawns `Particle` entities |
| **UI (immediate mode)** | `Widget.*`, `UIState.h`, `UIStateManager.*`, `*UI.*`, `UITarget.*` | |

## Game: Metal Invasion (`MainLevel`)

| Concept | Where | Data |
|---|---|---|
| Level setup | `CreateMainLevel.cpp` | Ground, player base (`PlayerBaseComponent`, static body, `AIObstacle`), unit selector (`UITarget`), vector fields |
| Player units | `PlayerUnits.*`, `UnitControllerSystem.*` | `PlayerControlUnit {battalionId, health, selected, isTank, isWall}`. Soldiers shoot, support units mine crystals, tanks are a root entity with base and cannon child meshes |
| Enemies | `BasicEnemyUnit.*`, `EnemyControllerSystem.*` | `BasicEnemyUnit {health}`, enemy soldiers and tanks |
| Economy | `Crystal.*` | `CrystalDeposit {AmountOfCrystal}` |
| Projectiles | `Bullet.*`, `Laser.*`, `ProjectileControllerSystem.*`, `BulletColliders.*` | `TankBullet`, `Explosion`, `LaserProjectile` |
| Building | `BuildObstaclesSystem.*` | Wall entity follows the cursor (`GameState::ObstacleInCursor`) until placed |
| Round flow | `GameRoundControllerSystem.*`, `GameState.h` | `GameState` resource: Spawn → Preparation → Invasion, timers, crystals, difficulty, camera state |

## Relevant observations for a save system

* **No existing persistence**: nothing in the repo reads or writes game state.
* **`TypeID` numbers aren't stable**, so a save format must identify types by name.
* **Entity ids are stored inside components and resources** (`Transform` parent
  and children, `GameState::ObstacleInCursor`, `BlackBoard` target maps). This
  argues for restoring exact ids rather than remapping them.
* **The `EntityManager` couldn't list living entities.** Entities without
  components can't be told apart from free ids by signature.
* **Some state isn't data**: behaviour-tree node graphs (code and references),
  render caches, shader instances. These have to be rebuilt after a load, not
  saved.
* **Private physics state** (`RigidBody` mass/inertia, `Shape` type) can't be
  rebuilt from public fields.
* **Build portability**: the root CMake needs CMake 4.0 and only configures
  Windows and MacOS. Game sources rely on MSVC/libc++ transitive includes. Tests
  therefore need their own headless build.
