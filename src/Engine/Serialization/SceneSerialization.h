//---------------------------------------------------------------------------------
// SceneSerialization.h
//---------------------------------------------------------------------------------
//
// Serialize functions for the scene components (World/SceneComponents.h) and
// the registry of everything a scene file contains.
//
// What is saved
//   Components : Transform, RigidBody, Mesh, FragShaderTag, VertShaderTag,
//                Particle, Emitter, AIObstacle              (engine)
//                SceneObject, Shape2D, ScriptComponent,
//                PrefabLink                                 (scene)
//                + the project's reflected components registered with
//                ComponentCatalog (Reflection/ComponentCatalog.h)
//   Resources  : SceneSettings
//
// What is NOT saved (rebuilt at runtime)
//   - Script instances: the ScriptSystem recreates them from ScriptComponent /
//     SceneSettings when the scene plays
//   - Render meshes and shader instances (Shape2D::Built, Mesh::Loaded ...)
//   - Engine resources that are configuration or per frame data (Camera,
//     buffers, UIState, GameOptions)
//
#pragma once

#include "../World/SceneComponents.h"
#include "EngineSerialization.h"
#include "SerializationRegistry.h"

SERIALIZATION_ENUM_RANGE(Shape2DType, Shape2DType::Rectangle, Shape2DType::Polygon)

template <typename Archive>
void Serialize(Archive& ar, SceneObject& object)
{
    ar(object.Name, object.Tag);
}

template <typename Archive>
void Serialize(Archive& ar, Shape2D& shape)
{
    ar(shape.Type, shape.Width, shape.Height, shape.Sides, shape.Thickness, shape.Color);
    // The render mesh is rebuilt by the MeshHandler
    if constexpr (Archive::IsLoading)
        shape.Built = false;
}

template <typename Archive>
void Serialize(Archive& ar, ScriptComponent& script)
{
    ar(script.Script, script.Params);
}

template <typename Archive>
void Serialize(Archive& ar, PrefabLink& link)
{
    ar(link.Prefab);
}

template <typename Archive>
void Serialize(Archive& ar, SceneSettings& settings)
{
    ar(settings.Name, settings.SceneScript, settings.SceneParams);
    ar(settings.FieldWidth, settings.FieldHeight);
    ar(settings.CameraTarget, settings.CameraDistance);
}

namespace Serialization
{
    /**
     * \brief Register the engine components (transform, physics, rendering ...)
     */
    void RegisterEngineSerializers(SerializationRegistry& registry);

    /**
     * \brief Register the scene components and resources
     */
    void RegisterSceneSerializers(SerializationRegistry& registry);

    /**
     * \brief Registry with every engine + scene serializer, built once
     */
    const SerializationRegistry& GetSceneSerializationRegistry();

    /**
     * \brief The same registry, to add the project's own components at
     *        startup (ComponentCatalog::Register does it). Register before
     *        loading scenes that contain them
     */
    SerializationRegistry& SceneSerializationRegistry();
} // namespace Serialization
