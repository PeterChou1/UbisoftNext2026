#include "ComponentCatalog.h"

#include "../World/SceneCamera.h"

ComponentCatalog& ComponentCatalog::Get()
{
    // The engine's own reflected components are always available
    static ComponentCatalog catalog = [] {
        ComponentCatalog engine;
        engine.Register<GameCamera>("GameCamera", "Makes the object the scene's game camera");
        return engine;
    }();
    return catalog;
}
