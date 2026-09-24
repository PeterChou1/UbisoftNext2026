#include "GameSerialization.h"

namespace Serialization
{
    // The names below are written into save files. Never rename them, bump the
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
        registry.RegisterComponent<UITarget>("UITarget");
        registry.RegisterComponent<AIObstacle>("AIObstacle");
    }

    void RegisterGameSerializers(SerializationRegistry& registry)
    {
        registry.RegisterComponent<PlayerControlUnit>("PlayerControlUnit");
        registry.RegisterComponent<BasicEnemyUnit>("BasicEnemyUnit");
        registry.RegisterComponent<CrystalDeposit>("CrystalDeposit");
        registry.RegisterComponent<PlayerBaseComponent>("PlayerBase");
        registry.RegisterComponent<TankBullet>("TankBullet");
        registry.RegisterComponent<Explosion>("Explosion");
        registry.RegisterComponent<LaserProjectile>("LaserProjectile");

        registry.RegisterResource<GameState>("GameState");
        registry.RegisterResource<BlackBoard>("BlackBoard");
        registry.RegisterResource<UIState>("UIState");
    }

    const SerializationRegistry& GetGameSerializationRegistry()
    {
        static const SerializationRegistry registry = [] {
            SerializationRegistry r;
            RegisterEngineSerializers(r);
            RegisterGameSerializers(r);
            return r;
        }();
        return registry;
    }
} // namespace Serialization
