#include "SceneObjects.h"

#include "../Camera.h"
#include "../ECSManager.h"
#include "../FragShaderTag.h"
#include "../Mesh.h"
#include "../RigidBody.h"
#include "../Transform.h"
#include "../VertShaderTag.h"
#include "ShapeGeometry.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

extern ECSManager ECS;

namespace SceneObjects
{
    namespace
    {
        constexpr float PI_F = 3.14159265f;
        constexpr float DEG_TO_RAD = PI_F / 180.0f;
        constexpr float RAD_TO_DEG = 180.0f / PI_F;
        // Camera looks at the field from this direction (tilted, from -Z)
        const Vec3 VIEW_DIRECTION = Vec3(0.0f, 1.0f, -0.55f).Normalize();
        // Footprint used for bodies / picking of model objects
        constexpr float MODEL_RADIUS = 0.5f;

        // Radius around an empty's position that picks it
        constexpr float EMPTY_PICK_RADIUS = 0.4f;

        Transform MakeTransform(const Vec3& position, float yawDegrees)
        {
            Transform t(position, Quat(Vec3(0, 1, 0), yawDegrees * DEG_TO_RAD));
            t.Plane = XZ;
            return t;
        }

        std::vector<Vec2> CircleOutline(float diameter)
        {
            Shape2D circle;
            circle.Type = Shape2DType::Circle;
            circle.Width = diameter;
            return ShapeGeometry::Outline(circle);
        }

        // The object's footprint: its outline (local x, z) and the shape the
        // body gets without a ColliderShape (Auto)
        struct Footprint
        {
            std::vector<Vec2> Outline;
            ColliderShapeType Auto = ColliderShapeType::Circle;
            float Width = 1.0f;
            float Height = 1.0f;
            float Radius = 0.5f;
        };

        Footprint FootprintOf(Entity entity)
        {
            Footprint footprint;
            if (!ECS.HasComponent<Shape2D>(entity))
            {
                // Models and empties: a circle of MODEL_RADIUS scaled
                float scale = ECS.GetComponent<Transform>(entity).LocalScale.X;
                footprint.Radius = std::max(MODEL_RADIUS * scale, 0.005f);
                footprint.Width = footprint.Height = footprint.Radius * 2.0f;
                footprint.Outline = CircleOutline(footprint.Width);
                return footprint;
            }
            const Shape2D& shape = ECS.GetComponent<Shape2D>(entity);
            footprint.Outline = ShapeGeometry::Outline(shape);
            footprint.Width = std::max(shape.Width, 0.01f);
            switch (shape.Type)
            {
            case Shape2DType::Rectangle:
                footprint.Auto = ColliderShapeType::Box;
                footprint.Height = std::max(shape.Height, 0.01f);
                footprint.Radius = std::max(footprint.Width, footprint.Height) * 0.5f;
                return footprint;
            case Shape2DType::Circle:
                footprint.Height = footprint.Width;
                footprint.Radius = footprint.Width * 0.5f;
                return footprint;
            default:
                break;
            }
            // Polygons: the box and circle around the outline
            footprint.Auto = ColliderShapeType::Polygon;
            Vec2 min = footprint.Outline.front(), max = min;
            float radius = 0.0f;
            for (const Vec2& p : footprint.Outline)
            {
                min = Vec2(std::min(min.X, p.X), std::min(min.Y, p.Y));
                max = Vec2(std::max(max.X, p.X), std::max(max.Y, p.Y));
                radius = std::max(radius, std::sqrt(p.X * p.X + p.Y * p.Y));
            }
            footprint.Width = std::max(max.X - min.X, 0.01f);
            footprint.Height = std::max(max.Y - min.Y, 0.01f);
            footprint.Radius = std::max(radius, 0.005f);
            return footprint;
        }

        RigidBody BuildBody(Entity entity)
        {
            Footprint footprint = FootprintOf(entity);
            ColliderShapeType type = footprint.Auto;
            float scale = 1.0f;
            if (ECS.HasComponent<ColliderShape>(entity))
            {
                const ColliderShape& collider = ECS.GetComponent<ColliderShape>(entity);
                if (collider.Type != ColliderShapeType::Auto)
                    type = collider.Type;
                scale = std::clamp(collider.Scale, 0.1f, 5.0f);
            }
            switch (type)
            {
            case ColliderShapeType::Box:
                return RigidBody(footprint.Width * scale, footprint.Height * scale);
            case ColliderShapeType::Polygon: {
                std::vector<Vec2> points = footprint.Outline;
                for (Vec2& p : points)
                    p = p * scale;
                return RigidBody(points);
            }
            default:
                return RigidBody(footprint.Radius * scale);
            }
        }

        // After the shape or collider changed: a body is rebuilt to match
        void RebuildBody(Entity entity)
        {
            BodyType body = GetBodyType(entity);
            if (body != BodyType::None)
                SetBodyType(entity, body);
        }

        bool HasTransform(Entity e)
        {
            return e != NULL_ENTITY && ECS.IsEntityAlive(e) && ECS.HasComponent<Transform>(e);
        }

        void Unlink(Entity child)
        {
            Transform& t = ECS.GetComponent<Transform>(child);
            if (HasTransform(t.Parent))
            {
                std::vector<Entity>& siblings = ECS.GetComponent<Transform>(t.Parent).Children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), child),
                               siblings.end());
            }
            t.Parent = NULL_ENTITY;
        }
    } // namespace

    const char* BodyTypeName(BodyType type)
    {
        switch (type)
        {
        case BodyType::Static:
            return "Static";
        case BodyType::Dynamic:
            return "Dynamic";
        case BodyType::Trigger:
            return "Trigger";
        default:
            return "None";
        }
    }

    Entity CreateShape(const ShapeDesc& desc)
    {
        Entity e = ECS.CreateEntity();
        ECS.AddComponent<Transform>(e, MakeTransform(desc.Position, desc.YawDegrees));
        ECS.AddComponent<SceneObject>(
                e, {desc.Name.empty() ? UniqueName("Shape") : desc.Name, desc.Tag});
        Shape2D shape = desc.Shape;
        shape.Built = false;
        ECS.AddComponent<Shape2D>(e, shape);
        ECS.AddComponent<FragShaderTag>(e, FragShaderTag(ShapeShaderID));
        SetBodyType(e, desc.Body);
        if (!desc.Script.empty())
            ECS.AddComponent<ScriptComponent>(e, {desc.Script, desc.ScriptParams});
        return e;
    }

    Entity CreateModel(const std::string& name,
                       const std::string& model,
                       const Vec3& position,
                       float yawDegrees,
                       float scale)
    {
        Entity e = ECS.CreateEntity();
        Transform t = MakeTransform(position, yawDegrees);
        t.Scale(scale);
        ECS.AddComponent<Transform>(e, t);
        ECS.AddComponent<SceneObject>(e, {name.empty() ? UniqueName(model) : name, ""});
        ECS.AddComponent<Mesh>(e, Mesh(model));
        ECS.AddComponent<FragShaderTag>(e, FragShaderTag(BlinnPhongID));
        return e;
    }

    Entity CreateEmpty(const std::string& name, const Vec3& position, float yawDegrees)
    {
        Entity e = ECS.CreateEntity();
        ECS.AddComponent<Transform>(e, MakeTransform(position, yawDegrees));
        ECS.AddComponent<SceneObject>(e, {name.empty() ? UniqueName("Empty") : name, ""});
        return e;
    }

    bool IsEmpty(Entity entity)
    {
        return ECS.IsEntityAlive(entity) && ECS.HasComponent<Transform>(entity) &&
               !ECS.HasComponent<Shape2D>(entity) && !ECS.HasComponent<Mesh>(entity);
    }

    bool SetParent(Entity child, Entity parent)
    {
        if (!HasTransform(child) || (parent != NULL_ENTITY && !HasTransform(parent)))
            return false;
        if (parent == child || IsAncestor(child, parent))
            return false;
        Transform& t = ECS.GetComponent<Transform>(child);
        if (t.Parent == parent)
            return true;

        // Where the child is in the world, and the new parent's frame
        Transform world = t.GetWorldTransform();
        Unlink(child);
        Transform::Pose frame{Vec3(0, 0, 0), Quat(0, 0, 0, 1), Vec3(1, 1, 1)};
        if (parent != NULL_ENTITY)
        {
            Transform& p = ECS.GetComponent<Transform>(parent);
            p.Children.push_back(child);
            t.Parent = parent;
            frame = t.ParentPose();
        }
        // Same world pose, expressed in the parent's frame
        auto unscale = [](float v, float s) { return std::fabs(s) > 1e-6f ? v / s : v; };
        Quat inverse = frame.Rotation.Inverse();
        Vec3 offset = inverse.RotatePoint(world.LocalPosition - frame.Position);
        Vec3 position(unscale(offset.X, frame.Scale.X),
                      unscale(offset.Y, frame.Scale.Y),
                      unscale(offset.Z, frame.Scale.Z));
        Vec3 scale(unscale(world.LocalScale.X, frame.Scale.X),
                   unscale(world.LocalScale.Y, frame.Scale.Y),
                   unscale(world.LocalScale.Z, frame.Scale.Z));
        Quat rotation = inverse * world.LocalRotation;
        rotation.Normalize();
        t.SetLocalPose(position, rotation, scale);
        return true;
    }

    Entity GetParent(Entity entity)
    {
        if (!HasTransform(entity))
            return NULL_ENTITY;
        Entity parent = ECS.GetComponent<Transform>(entity).Parent;
        return HasTransform(parent) ? parent : NULL_ENTITY;
    }

    std::vector<Entity> GetChildren(Entity entity)
    {
        std::vector<Entity> result;
        if (!HasTransform(entity))
            return result;
        for (Entity child : ECS.GetComponent<Transform>(entity).Children)
        {
            if (HasTransform(child))
                result.push_back(child);
        }
        return result;
    }

    bool IsAncestor(Entity ancestor, Entity entity)
    {
        if (ancestor == NULL_ENTITY)
            return false;
        Entity it = GetParent(entity);
        // Bounded walk: never loops forever on a damaged hierarchy
        for (std::size_t steps = 0; it != NULL_ENTITY && steps <= MAX_ENTITIES; ++steps)
        {
            if (it == ancestor)
                return true;
            it = GetParent(it);
        }
        return false;
    }

    int RepairHierarchy()
    {
        int fixes = 0;
        std::vector<Entity> objects;
        for (Entity e : ECS.GetLivingEntities())
        {
            if (ECS.HasComponent<Transform>(e))
                objects.push_back(e);
        }
        for (Entity e : objects)
        {
            Transform& t = ECS.GetComponent<Transform>(e);
            // Children: alive, with a Transform, pointing back, listed once
            std::vector<Entity> kept;
            for (Entity child : t.Children)
            {
                bool valid = HasTransform(child) &&
                             ECS.GetComponent<Transform>(child).Parent == e &&
                             std::find(kept.begin(), kept.end(), child) == kept.end();
                if (valid)
                    kept.push_back(child);
                else
                    ++fixes;
            }
            t.Children = std::move(kept);
        }
        for (Entity e : objects)
        {
            Transform& t = ECS.GetComponent<Transform>(e);
            if (t.Parent == NULL_ENTITY)
                continue;
            if (!HasTransform(t.Parent) || t.Parent == e)
            {
                t.Parent = NULL_ENTITY;
                ++fixes;
                continue;
            }
            std::vector<Entity>& siblings = ECS.GetComponent<Transform>(t.Parent).Children;
            if (std::find(siblings.begin(), siblings.end(), e) == siblings.end())
            {
                siblings.push_back(e);
                ++fixes;
            }
        }
        // Loops: walking up from an entity must end at a root
        for (Entity e : objects)
        {
            std::vector<Entity> path;
            Entity it = e;
            while (it != NULL_ENTITY)
            {
                if (std::find(path.begin(), path.end(), it) != path.end())
                {
                    // `it` closes a loop: cut it from its parent
                    Unlink(it);
                    ++fixes;
                    break;
                }
                path.push_back(it);
                it = ECS.GetComponent<Transform>(it).Parent;
            }
        }
        if (fixes > 0)
        {
            for (Entity e : objects)
                ECS.GetComponent<Transform>(e).IsDirty = true;
        }
        return fixes;
    }

    void SetColliderShape(Entity entity, ColliderShapeType type, float scale)
    {
        ColliderShape collider{type, std::clamp(scale, 0.1f, 5.0f)};
        if (ECS.HasComponent<ColliderShape>(entity))
            ECS.GetComponent<ColliderShape>(entity) = collider;
        else
            ECS.AddComponent<ColliderShape>(entity, collider);
        RebuildBody(entity);
    }

    ColliderShape ColliderShapeOf(Entity entity)
    {
        return ECS.HasComponent<ColliderShape>(entity) ? ECS.GetComponent<ColliderShape>(entity)
                                                       : ColliderShape{};
    }

    ColliderShapeType EffectiveColliderShape(Entity entity)
    {
        ColliderShape collider = ColliderShapeOf(entity);
        return collider.Type != ColliderShapeType::Auto ? collider.Type : FootprintOf(entity).Auto;
    }

    std::vector<Vec3> ColliderOutline(Entity entity, int circleSegments)
    {
        std::vector<Vec3> outline;
        if (!ECS.HasComponent<RigidBody>(entity) || !ECS.HasComponent<Transform>(entity))
            return outline;
        // Where physics puts the body for the transform as it is now (the
        // body's own state is only synced while the world simulates)
        RigidBody body = ECS.GetComponent<RigidBody>(entity);
        Transform& transform = ECS.GetComponent<Transform>(entity);
        body.SyncTransform(transform);
        body.RecomputeGeometry();
        float y = transform.GetWorldPosition().Y;
        if (body.Shape.GetShapeType() == CircleShape)
        {
            const int segments = std::max(circleSegments, 3);
            for (int i = 0; i < segments; ++i)
            {
                float angle = 2.0f * PI_F * static_cast<float>(i) / static_cast<float>(segments);
                outline.push_back(Vec3(body.Position.X + body.Shape.Radius * std::cos(angle),
                                       y,
                                       body.Position.Y + body.Shape.Radius * std::sin(angle)));
            }
            return outline;
        }
        for (const Vec2& p : body.Shape.PolygonPoints)
            outline.push_back(Vec3(p.X, y, p.Y));
        return outline;
    }

    float ColliderHeight(Entity entity)
    {
        if (ECS.HasComponent<Shape2D>(entity))
            return std::max(ECS.GetComponent<Shape2D>(entity).Thickness, 0.05f);
        return std::max(ECS.GetComponent<Transform>(entity).LocalScale.Y * MODEL_RADIUS * 2.0f,
                        0.1f);
    }

    void SetBodyType(Entity entity, BodyType type)
    {
        if (ECS.HasComponent<RigidBody>(entity))
            ECS.RemoveComponent<RigidBody>(entity);
        if (type == BodyType::None)
            return;
        RigidBody body = BuildBody(entity);
        if (type == BodyType::Static)
            body.SetStatic();
        if (type == BodyType::Trigger)
            body.Collidable = false;
        ECS.AddComponent<RigidBody>(entity, body);
    }

    BodyType GetBodyType(Entity entity)
    {
        if (!ECS.HasComponent<RigidBody>(entity))
            return BodyType::None;
        const RigidBody& body = ECS.GetComponent<RigidBody>(entity);
        if (!body.Collidable)
            return BodyType::Trigger;
        return body.IsStatic() ? BodyType::Static : BodyType::Dynamic;
    }

    void ShapeChanged(Entity entity)
    {
        if (ECS.HasComponent<Shape2D>(entity))
            ECS.GetComponent<Shape2D>(entity).Built = false;
        RebuildBody(entity);
    }

    void SetYaw(Entity entity, float degrees)
    {
        ECS.GetComponent<Transform>(entity).SetGlobalRotation(
                Quat(Vec3(0, 1, 0), degrees * DEG_TO_RAD));
    }

    float GetYaw(Entity entity)
    {
        float degrees =
                ECS.GetComponent<Transform>(entity).GetWorldRotation().GetPitch2D() * RAD_TO_DEG;
        degrees = std::fmod(degrees, 360.0f);
        if (degrees < 0.0f)
            degrees += 360.0f;
        // Remove float noise such as 89.99998
        float rounded = std::round(degrees * 100.0f) / 100.0f;
        return rounded >= 360.0f ? 0.0f : rounded;
    }

    Vec3 GetPosition(Entity entity)
    {
        return ECS.GetComponent<Transform>(entity).GetWorldPosition();
    }

    void SetPosition(Entity entity, const Vec3& position)
    {
        ECS.GetComponent<Transform>(entity).SetWorldPosition(position);
    }

    Entity FindByName(const std::string& name)
    {
        for (Entity e : ECS.Visit<SceneObject>())
        {
            if (ECS.GetComponent<SceneObject>(e).Name == name)
                return e;
        }
        return NULL_ENTITY;
    }

    std::vector<Entity> FindByTag(const std::string& tag)
    {
        std::vector<Entity> result;
        for (Entity e : ECS.Visit<SceneObject>())
        {
            if (ECS.GetComponent<SceneObject>(e).Tag == tag)
                result.push_back(e);
        }
        return result;
    }

    std::string UniqueName(const std::string& base)
    {
        // Every name in one pass: trying "base 2", "base 3" ... with a scan of
        // the scene for each made creating n objects O(n^3)
        std::unordered_set<std::string> names;
        for (Entity e : ECS.Visit<SceneObject>())
            names.insert(ECS.GetComponent<SceneObject>(e).Name);
        if (names.count(base) == 0)
            return base;
        for (int i = 2;; ++i)
        {
            std::string candidate = base + " " + std::to_string(i);
            if (names.count(candidate) == 0)
                return candidate;
        }
    }

    void Destroy(Entity entity)
    {
        if (!ECS.IsEntityAlive(entity))
            return;
        if (ECS.HasComponent<Transform>(entity))
        {
            Unlink(entity);
            std::vector<Entity> children = ECS.GetComponent<Transform>(entity).Children;
            for (Entity child : children)
            {
                // Children already destroyed on their own are skipped
                if (HasTransform(child) && ECS.GetComponent<Transform>(child).Parent == entity)
                {
                    ECS.GetComponent<Transform>(child).Parent = NULL_ENTITY;
                    Destroy(child);
                }
            }
        }
        ECS.DestroyEntity(entity);
    }

    bool Contains(Entity entity, const Vec3& worldPoint, float margin)
    {
        if (!ECS.HasComponent<Transform>(entity))
            return false;
        // World frame of the object (parents included)
        Transform t = ECS.GetComponent<Transform>(entity).GetWorldTransform();
        if (IsEmpty(entity))
        {
            Vec3 d = worldPoint - t.LocalPosition;
            float radius = EMPTY_PICK_RADIUS + margin;
            return d.X * d.X + d.Z * d.Z <= radius * radius;
        }
        // Into the object's local frame (undoes position, rotation and scale).
        // Transform::Inverse is a rigid inverse (transpose) that is wrong for
        // scaled objects, so project on the Affine axes instead: the columns
        // of rotation * scale are orthogonal with length = scale
        Vec3 d = worldPoint - Vec3(t.Affine[0][3], t.Affine[1][3], t.Affine[2][3]);
        float axes[3];
        for (int i = 0; i < 3; ++i)
        {
            Vec3 axis(t.Affine[0][i], t.Affine[1][i], t.Affine[2][i]);
            float lengthSqr = axis.Dot(axis);
            axes[i] = lengthSqr > 0.0f ? axis.Dot(d) / lengthSqr : 0.0f;
        }
        Vec3 local(axes[0], axes[1], axes[2]);
        if (ECS.HasComponent<Shape2D>(entity))
        {
            return ShapeGeometry::Contains(
                    ECS.GetComponent<Shape2D>(entity), Vec2(local.X, local.Z), margin);
        }
        float scale = t.LocalScale.X;
        float radius = MODEL_RADIUS + margin / std::max(scale, 0.01f);
        return local.X * local.X + local.Z * local.Z <= radius * radius;
    }

    std::vector<Vec3> WorldOutline(Entity entity)
    {
        std::vector<Vec3> outline;
        // Empties have no footprint (the editor draws them as a cross)
        if (!ECS.HasComponent<Transform>(entity) || IsEmpty(entity))
            return outline;
        Transform t = ECS.GetComponent<Transform>(entity).GetWorldTransform();
        std::vector<Vec2> local;
        if (ECS.HasComponent<Shape2D>(entity))
            local = ShapeGeometry::Outline(ECS.GetComponent<Shape2D>(entity));
        else
            local = CircleOutline(MODEL_RADIUS * 2.0f);
        for (const Vec2& p : local)
            outline.push_back(t.Affine * Vec3(p.X, 0.0f, p.Y));
        return outline;
    }

    void SetFragmentShader(Entity entity, FragShaderTypeID fragment)
    {
        if (ECS.HasComponent<FragShaderTag>(entity))
            ECS.GetComponent<FragShaderTag>(entity).FragAssetId = fragment;
        else
            ECS.AddComponent<FragShaderTag>(entity, FragShaderTag(fragment));
    }

    void SetVertexShader(Entity entity, VertShaderTypeID vertex)
    {
        if (ECS.HasComponent<VertShaderTag>(entity))
            ECS.GetComponent<VertShaderTag>(entity).VertAssetId = vertex;
        else if (vertex != DefaultVertShaderID)
            ECS.AddComponent<VertShaderTag>(entity, VertShaderTag(vertex));
    }

    void SetShaders(Entity entity, FragShaderTypeID fragment, VertShaderTypeID vertex)
    {
        SetFragmentShader(entity, fragment);
        SetVertexShader(entity, vertex);
    }

    FragShaderTypeID FragmentShaderOf(Entity entity)
    {
        return ECS.HasComponent<FragShaderTag>(entity)
                       ? ECS.GetComponent<FragShaderTag>(entity).FragAssetId
                       : DefaultFragShaderID;
    }

    VertShaderTypeID VertexShaderOf(Entity entity)
    {
        return ECS.HasComponent<VertShaderTag>(entity)
                       ? ECS.GetComponent<VertShaderTag>(entity).VertAssetId
                       : DefaultVertShaderID;
    }

    void ApplyCamera(Camera& camera, const Vec3& target, float distance)
    {
        camera.SetPositionAndOrientation(target + VIEW_DIRECTION * distance, target, {0, 1, 0});
    }
} // namespace SceneObjects
