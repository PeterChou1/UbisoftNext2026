#include "SceneSerialization.h"

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
        registry.RegisterResource<SceneSettings>("SceneSettings");
    }

    SerializationRegistry& SceneSerializationRegistry()
    {
        static SerializationRegistry registry = [] {
            SerializationRegistry r;
            RegisterEngineSerializers(r);
            RegisterSceneSerializers(r);
            return r;
        }();
        return registry;
    }

    const SerializationRegistry& GetSceneSerializationRegistry()
    {
        return SceneSerializationRegistry();
    }
} // namespace Serialization
