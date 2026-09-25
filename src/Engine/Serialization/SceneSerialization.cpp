#include "SceneSerialization.h"

#include "../Log.h"
#include "../World/SceneObjects.h"

extern ECSManager ECS;

namespace Serialization
{
    // The names below are written into scene files. Never rename them, bump the
    // version passed to Register... instead when a layout changes.

    void RegisterEngineSerializers(SerializationRegistry& registry)
    {
        registry.RegisterComponent<Transform>("Transform");
        registry.RegisterComponent<RigidBody>("RigidBody");
        registry.RegisterComponent<Mesh>("Mesh");
        registry.RegisterComponent<FragShaderTag>("FragShaderTag");
        registry.RegisterComponent<VertShaderTag>("VertShaderTag");
        registry.RegisterComponent<Particle>("Particle");
        registry.RegisterComponent<Emitter>("Emitter");
        registry.RegisterComponent<AIObstacle>("AIObstacle");
    }

    void RegisterSceneSerializers(SerializationRegistry& registry)
    {
        registry.RegisterComponent<SceneObject>("SceneObject");
        registry.RegisterComponent<Shape2D>("Shape2D");
        registry.RegisterComponent<ScriptComponent>("ScriptComponent");
        registry.RegisterComponent<PrefabLink>("PrefabLink");
        registry.RegisterResource<SceneSettings>("SceneSettings");
    }

    SerializationRegistry& SceneSerializationRegistry()
    {
        static SerializationRegistry registry = [] {
            SerializationRegistry r;
            RegisterEngineSerializers(r);
            RegisterSceneSerializers(r);
            // A damaged or hand edited file can hold parent links that do not
            // match: fix them so no system follows a dead parent or a loop
            r.AddPostLoadCallback([](ECSManager& ecs) {
                if (&ecs != &ECS)
                    return;
                if (int fixes = SceneObjects::RepairHierarchy(); fixes > 0)
                    LOG_WARN("Serialization", "Repaired %d broken parent / child links", fixes);
            });
            return r;
        }();
        return registry;
    }

    const SerializationRegistry& GetSceneSerializationRegistry()
    {
        return SceneSerializationRegistry();
    }
} // namespace Serialization
