//---------------------------------------------------------------------------------
// SceneObjects.h
//---------------------------------------------------------------------------------
//
// Creating, finding and modifying scene objects. Used by the scene editor, by
// scripts at runtime (spawning, lookups) and by tests, so an object built in
// any of these places is made of exactly the same components.
//
// All functions operate on the global ECS instance.
//
#pragma once

#include "../Entity.h"
#include "../Vec3.h"
#include "SceneComponents.h"

#include <map>
#include <string>
#include <vector>

class Camera;

namespace SceneObjects
{
    /**
     * \brief Physics behaviour of an object
     *   None     no RigidBody (decoration, ground)
     *   Static   immovable collider (walls)
     *   Dynamic  moved by the physics simulation (set its velocity)
     *   Trigger  detects contacts (script events) but never pushes anything
     */
    enum class BodyType
    {
        None,
        Static,
        Dynamic,
        Trigger,
        Count
    };

    const char* BodyTypeName(BodyType type);

    struct ShapeDesc
    {
        std::string Name;
        std::string Tag;
        Shape2D Shape;
        Vec3 Position = {0, 0, 0};
        float YawDegrees = 0.0f;
        BodyType Body = BodyType::None;
        // Optional object script (registered name) and parameter overrides
        std::string Script;
        std::map<std::string, float> ScriptParams;
    };

    /**
     * \brief Create a shape object: Transform, SceneObject, Shape2D,
     *        FragShaderTag and, depending on the body type, a RigidBody
     */
    Entity CreateShape(const ShapeDesc& desc);

    /**
     * \brief Create an object showing a 3D model from data/models
     */
    Entity CreateModel(const std::string& name,
                       const std::string& model,
                       const Vec3& position,
                       float yawDegrees = 0.0f,
                       float scale = 1.0f);

    /**
     * \brief Replace the object's RigidBody so it matches its shape and the
     *        requested body type (call after changing the shape or its size)
     */
    void SetBodyType(Entity entity, BodyType type);

    BodyType GetBodyType(Entity entity);

    /**
     * \brief Rebuild the render mesh (and body) after editing the Shape2D
     */
    void ShapeChanged(Entity entity);

    void SetYaw(Entity entity, float degrees);

    /**
     * \brief Rotation around the up axis in degrees, in [0, 360)
     */
    float GetYaw(Entity entity);

    Vec3 GetPosition(Entity entity);

    void SetPosition(Entity entity, const Vec3& position);

    /**
     * \brief First object with that name (NULL_ENTITY if none)
     */
    Entity FindByName(const std::string& name);

    std::vector<Entity> FindByTag(const std::string& tag);

    /**
     * \brief "base", "base 2", "base 3" ... the first name not used yet
     */
    std::string UniqueName(const std::string& base);

    /**
     * \brief Destroy an entity and all of its Transform children
     */
    void Destroy(Entity entity);

    /**
     * \brief True if the world point (x, z) lies on the object
     */
    bool Contains(Entity entity, const Vec3& worldPoint, float margin = 0.0f);

    /**
     * \brief Footprint of the object in world space (x, z), for overlays
     */
    std::vector<Vec3> WorldOutline(Entity entity);

    /**
     * \brief Place the camera like the scene settings ask (fixed tilt)
     */
    void ApplyCamera(Camera& camera, const Vec3& target, float distance);
} // namespace SceneObjects
