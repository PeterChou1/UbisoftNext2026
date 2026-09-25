#include "ComponentCatalog.h"

#include "../World/ColliderShape.h"
#include "../World/SceneCamera.h"
#include "../World/SceneLight.h"

ComponentCatalog& ComponentCatalog::Get()
{
    // The engine's own reflected components are always available
    static ComponentCatalog catalog = [] {
        ComponentCatalog engine;
        engine.Register<GameCamera>("GameCamera", "Makes the object the scene's game camera");
        engine.Register<SceneLight>("SceneLight", "Makes the object the scene's light");
        // Edited in the RigidBody section; listed so copies and prefabs keep it
        engine.Register<ColliderShape>("Collider", "Shape of the physics body");
        return engine;
    }();
    return catalog;
}
