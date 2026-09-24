#include "SceneObjects.h"

#include "../Camera.h"
#include "../ECSManager.h"
#include "../FragShaderTag.h"
#include "../Mesh.h"
#include "../RigidBody.h"
#include "../Transform.h"
#include "ShapeGeometry.h"

#include <algorithm>
#include <cmath>

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

        Transform MakeTransform(const Vec3& position, float yawDegrees)
        {
            Transform t(position, Quat(Vec3(0, 1, 0), yawDegrees * DEG_TO_RAD));
            t.Plane = XZ;
            return t;
        }

        RigidBody BuildBody(Entity entity)
        {
            if (ECS.HasComponent<Shape2D>(entity))
            {
                const Shape2D& shape = ECS.GetComponent<Shape2D>(entity);
                if (shape.Type == Shape2DType::Rectangle)
                    return RigidBody(std::max(shape.Width, 0.01f), std::max(shape.Height, 0.01f));
                if (shape.Type == Shape2DType::Circle)
                    return RigidBody(std::max(shape.Width, 0.01f) * 0.5f);
                return RigidBody(ShapeGeometry::Outline(shape));
            }
            float scale = ECS.GetComponent<Transform>(entity).LocalScale.X;
            return RigidBody(MODEL_RADIUS * scale);
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
        ECS.AddComponent<SceneObject>(e, {desc.Name.empty() ? UniqueName("Shape") : desc.Name, desc.Tag});
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
        BodyType body = GetBodyType(entity);
        if (body != BodyType::None)
            SetBodyType(entity, body);
    }

    void SetYaw(Entity entity, float degrees)
    {
        ECS.GetComponent<Transform>(entity).SetGlobalRotation(
                Quat(Vec3(0, 1, 0), degrees * DEG_TO_RAD));
    }

    float GetYaw(Entity entity)
    {
        float degrees = ECS.GetComponent<Transform>(entity).LocalRotation.GetPitch2D() * RAD_TO_DEG;
        degrees = std::fmod(degrees, 360.0f);
        if (degrees < 0.0f)
            degrees += 360.0f;
        // Remove float noise such as 89.99998
        float rounded = std::round(degrees * 100.0f) / 100.0f;
        return rounded >= 360.0f ? 0.0f : rounded;
    }

    Vec3 GetPosition(Entity entity)
    {
        return ECS.GetComponent<Transform>(entity).LocalPosition;
    }

    void SetPosition(Entity entity, const Vec3& position)
    {
        ECS.GetComponent<Transform>(entity).SetLocalPosition(position);
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
        if (FindByName(base) == NULL_ENTITY)
            return base;
        for (int i = 2;; ++i)
        {
            std::string candidate = base + " " + std::to_string(i);
            if (FindByName(candidate) == NULL_ENTITY)
                return candidate;
        }
    }

    void Destroy(Entity entity)
    {
        if (!ECS.IsEntityAlive(entity))
            return;
        if (ECS.HasComponent<Transform>(entity))
        {
            std::vector<Entity> children = ECS.GetComponent<Transform>(entity).Children;
            for (Entity child : children)
                Destroy(child);
        }
        ECS.DestroyEntity(entity);
    }

    bool Contains(Entity entity, const Vec3& worldPoint, float margin)
    {
        if (!ECS.HasComponent<Transform>(entity))
            return false;
        Transform& t = ECS.GetComponent<Transform>(entity);
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
        if (!ECS.HasComponent<Transform>(entity))
            return outline;
        Transform& t = ECS.GetComponent<Transform>(entity);
        std::vector<Vec2> local;
        if (ECS.HasComponent<Shape2D>(entity))
        {
            local = ShapeGeometry::Outline(ECS.GetComponent<Shape2D>(entity));
        }
        else
        {
            Shape2D circle;
            circle.Type = Shape2DType::Circle;
            circle.Width = MODEL_RADIUS * 2.0f;
            local = ShapeGeometry::Outline(circle);
        }
        for (const Vec2& p : local)
            outline.push_back(t.Affine * Vec3(p.X, 0.0f, p.Y));
        return outline;
    }

    void ApplyCamera(Camera& camera, const Vec3& target, float distance)
    {
        camera.SetPositionAndOrientation(target + VIEW_DIRECTION * distance, target, {0, 1, 0});
    }
} // namespace SceneObjects
