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

#include "../Assets.h"
#include "../Entity.h"
#include "../Vec3.h"
#include "ColliderShape.h"
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
     * \brief Create an empty object: just a Transform and a SceneObject. It
     *        draws nothing in the game; use it to group objects (as their
     *        parent), as a spawn point or marker, or to hold components and
     *        scripts. The editor shows it as a cross
     */
    Entity CreateEmpty(const std::string& name, const Vec3& position, float yawDegrees = 0.0f);

    /**
     * \brief True for objects without a shape or a model (CreateEmpty)
     */
    bool IsEmpty(Entity entity);

    // -- Hierarchy -------------------------------------------------------------
    //
    // Transform::Parent / Children link objects into a tree. A child's
    // Transform is relative to its parent: moving, turning or scaling the
    // parent carries the children along. Positions and yaws given to the
    // functions of this file are in world space.

    /**
     * \brief Make `child` a child of `parent` (NULL_ENTITY = a root object).
     *        The child keeps its place in the world. False (nothing changed)
     *        when the link would make a loop or an entity has no Transform
     */
    bool SetParent(Entity child, Entity parent);

    Entity GetParent(Entity entity);

    /**
     * \brief Children in their order (living entities with a Transform)
     */
    std::vector<Entity> GetChildren(Entity entity);

    /**
     * \brief True if `ancestor` is the parent of `entity`, or its parent's
     *        parent, and so on
     */
    bool IsAncestor(Entity ancestor, Entity entity);

    /**
     * \brief Fix broken links (e.g. from a hand edited file): parents that do
     *        not exist or do not list the child, children that are dead or
     *        belong to another parent, loops. Returns the number of fixes
     */
    int RepairHierarchy();

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
     * \brief Rotation around the up axis in degrees, in [0, 360), in the
     *        world (parents included). SetYaw also takes a world yaw
     */
    float GetYaw(Entity entity);

    /**
     * \brief World position (parents included)
     */
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
     * \brief Destroy an entity and all of its Transform children (it is
     *        removed from its parent's children first)
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

    // -- Colliders ------------------------------------------------------------------

    /**
     * \brief Shape of the object's physics body (ColliderShape.h); rebuilds
     *        the body when it has one
     */
    void SetColliderShape(Entity entity, ColliderShapeType type, float scale = 1.0f);
    ColliderShape ColliderShapeOf(Entity entity);
    /**
     * \brief The shape the body really has (Auto resolved)
     */
    ColliderShapeType EffectiveColliderShape(Entity entity);

    /**
     * \brief The body's collider in world space, at the object's height, as
     *        a closed loop (circles as `circleSegments` points), placed from
     *        the transform exactly as the physics system will place it.
     *        Empty without a RigidBody
     */
    std::vector<Vec3> ColliderOutline(Entity entity, int circleSegments = 24);
    /**
     * \brief How tall the collider is drawn (the shape's thickness)
     */
    float ColliderHeight(Entity entity);

    /**
     * \brief Shaders that draw a shape or a model: its FragShaderTag and
     *        VertShaderTag (added when missing). The renderer picks the change
     *        up on the next frame
     */
    void SetShaders(Entity entity, FragShaderTypeID fragment, VertShaderTypeID vertex);
    void SetFragmentShader(Entity entity, FragShaderTypeID fragment);
    void SetVertexShader(Entity entity, VertShaderTypeID vertex);

    /**
     * \brief The object's shaders (the defaults when it has no tag)
     */
    FragShaderTypeID FragmentShaderOf(Entity entity);
    VertShaderTypeID VertexShaderOf(Entity entity);

    /**
     * \brief Place the camera like the scene settings ask (fixed tilt)
     */
    void ApplyCamera(Camera& camera, const Vec3& target, float distance);
} // namespace SceneObjects
