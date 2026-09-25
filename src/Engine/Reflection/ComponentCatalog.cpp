#include "ComponentCatalog.h"

#include "../World/SceneCamera.h"
#include "../World/SceneLight.h"

ComponentCatalog& ComponentCatalog::Get()
{
    // The engine's own reflected components are always available
    static ComponentCatalog catalog = [] {
        ComponentCatalog engine;
        engine.Register<GameCamera>("GameCamera", "Makes the object the scene's game camera");
        engine.Register<SceneLight>("SceneLight", "Makes the object the scene's light");
        return engine;
    }();
    return catalog;
}
