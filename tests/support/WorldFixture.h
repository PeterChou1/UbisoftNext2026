//---------------------------------------------------------------------------------
// WorldFixture.h
//---------------------------------------------------------------------------------
//
// Shared helpers for the tests:
//   - FreshWorld():       empty world in the ScenePlayer scene (real engine path)
//   - BuildSampleWorld(): a scene using every feature (every shape type and
//                         body type, scripts with parameters, a model, a
//                         parent / child pair, a scene script, destroyed ids)
//   - WorldImage:         independent field by field copy of the world, compared
//                         with explicit bit exact comparisons, so the tests never
//                         use the serializer to verify the serializer
//
#pragma once

#include "ECSManager.h"
#include "GameManager.h"
#include "Mesh.h"
#include "RigidBody.h"
#include "Serialization/SceneSerialization.h"
#include "Serialization/WorldSerializer.h"
#include "TestEnvironment.h"
#include "TestFramework.h"
#include "World/SceneComponents.h"
#include "World/SceneObjects.h"
#include "World/ScenePlayer.h"

#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

extern ECSManager ECS;
extern GameManager GameSceneManager;

namespace Fixture
{
    //-----------------------------------------------------------------------------
    // Bit exact comparisons
    //-----------------------------------------------------------------------------

    inline bool Same(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

    inline bool Same(const Vec2& a, const Vec2& b) { return Same(a.X, b.X) && Same(a.Y, b.Y); }

    inline bool Same(const Vec3& a, const Vec3& b)
    {
        return Same(a.X, b.X) && Same(a.Y, b.Y) && Same(a.Z, b.Z);
    }

    inline bool Same(const Vec4& a, const Vec4& b)
    {
        return Same(a.X, b.X) && Same(a.Y, b.Y) && Same(a.Z, b.Z) && Same(a.W, b.W);
    }

    inline bool Same(const Quat& a, const Quat& b)
    {
        return Same(a.W, b.W) && Same(a.X, b.X) && Same(a.Y, b.Y) && Same(a.Z, b.Z);
    }

    inline bool Same(const Mat4& a, const Mat4& b)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (!Same(a.Rows[i], b.Rows[i]))
                return false;
        }
        return true;
    }

    inline bool Same(const std::vector<Vec2>& a, const std::vector<Vec2>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (!Same(a[i], b[i]))
                return false;
        }
        return true;
    }

    inline bool Same(const Transform& a, const Transform& b)
    {
        return a.Parent == b.Parent && a.Children == b.Children &&
               Same(a.LocalPosition, b.LocalPosition) && Same(a.LocalScale, b.LocalScale) &&
               Same(a.LocalRotation, b.LocalRotation) && Same(a.Affine, b.Affine) &&
               Same(a.Inverse, b.Inverse) && a.Plane == b.Plane && a.IsDirty == b.IsDirty;
    }

    inline bool Same(const AABB& a, const AABB& b)
    {
        return Same(a.OriginalMax, b.OriginalMax) && Same(a.OriginalMin, b.OriginalMin) &&
               Same(a.Max, b.Max) && Same(a.Min, b.Min);
    }

    inline bool Same(const Shape& a, const Shape& b)
    {
        return a.GetShapeType() == b.GetShapeType() && Same(a.Width, b.Width) &&
               Same(a.Height, b.Height) && Same(a.Radius, b.Radius) &&
               Same(a.PolygonPoints, b.PolygonPoints) && Same(a.EdgeNormals, b.EdgeNormals) &&
               Same(a.LocalSpacePoints, b.LocalSpacePoints) && Same(a.Max, b.Max) &&
               Same(a.Min, b.Min);
    }

    inline bool Same(const RigidBody& a, const RigidBody& b)
    {
        return a.IsIntersecting == b.IsIntersecting && a.Initialized == b.Initialized &&
               a.Collidable == b.Collidable && a.Category == b.Category && Same(a.Color, b.Color) &&
               Same(a.RigidBodyAABB, b.RigidBodyAABB) && Same(a.Shape, b.Shape) &&
               Same(a.Position, b.Position) && Same(a.Velocity, b.Velocity) &&
               Same(a.Force, b.Force) && Same(a.Angular, b.Angular) &&
               Same(a.AngularVelocity, b.AngularVelocity) && Same(a.AngularDelta, b.AngularDelta) &&
               Same(a.StaticFriction, b.StaticFriction) &&
               Same(a.DynamicFriction, b.DynamicFriction) && Same(a.InvMass(), b.InvMass()) &&
               Same(a.InvInertia(), b.InvInertia()) && Same(a.Restitution(), b.Restitution());
    }

    inline bool Same(const SceneObject& a, const SceneObject& b)
    {
        return a.Name == b.Name && a.Tag == b.Tag;
    }

    // Built is a runtime flag (reset on load), deliberately not compared
    inline bool Same(const Shape2D& a, const Shape2D& b)
    {
        return a.Type == b.Type && Same(a.Width, b.Width) && Same(a.Height, b.Height) &&
               a.Sides == b.Sides && Same(a.Thickness, b.Thickness) && Same(a.Color, b.Color);
    }

    inline bool Same(const ScriptComponent& a, const ScriptComponent& b)
    {
        if (a.Script != b.Script || a.Params.size() != b.Params.size())
            return false;
        for (const auto& kv : a.Params)
        {
            auto it = b.Params.find(kv.first);
            if (it == b.Params.end() || !Same(kv.second, it->second))
                return false;
        }
        return true;
    }

    inline bool Same(const Mesh& a, const Mesh& b) { return a.Model == b.Model; }

    inline bool Same(const FragShaderTag& a, const FragShaderTag& b)
    {
        return a.FragAssetId == b.FragAssetId;
    }

    inline bool Same(const SceneSettings& a, const SceneSettings& b)
    {
        return a.Name == b.Name && a.SceneScript == b.SceneScript &&
               a.SceneParams == b.SceneParams && Same(a.FieldWidth, b.FieldWidth) &&
               Same(a.FieldHeight, b.FieldHeight) && Same(a.CameraTarget, b.CameraTarget) &&
               Same(a.CameraDistance, b.CameraDistance);
    }

    //-----------------------------------------------------------------------------
    // WorldImage
    //-----------------------------------------------------------------------------

    struct WorldImage
    {
        std::vector<Entity> Living;
        std::vector<Entity> Available;
        std::map<Entity, Transform> Transforms;
        std::map<Entity, RigidBody> Bodies;
        std::map<Entity, SceneObject> Objects;
        std::map<Entity, Shape2D> Shapes;
        std::map<Entity, ScriptComponent> Scripts;
        std::map<Entity, Mesh> Meshes;
        std::map<Entity, FragShaderTag> Shaders;
        SceneSettings Settings;
    };

    template <typename T>
    void CaptureComponent(const std::vector<Entity>& living, std::map<Entity, T>& out)
    {
        for (Entity e : living)
        {
            if (ECS.HasComponent<T>(e))
                out.emplace(e, ECS.GetComponent<T>(e));
        }
    }

    inline WorldImage Capture()
    {
        WorldImage image;
        image.Living = ECS.GetLivingEntities();
        image.Available = ECS.GetAvailableEntities();
        CaptureComponent(image.Living, image.Transforms);
        CaptureComponent(image.Living, image.Bodies);
        CaptureComponent(image.Living, image.Objects);
        CaptureComponent(image.Living, image.Shapes);
        CaptureComponent(image.Living, image.Scripts);
        CaptureComponent(image.Living, image.Meshes);
        CaptureComponent(image.Living, image.Shaders);
        image.Settings = *ECS.GetResource<SceneSettings>();
        return image;
    }

    template <typename T>
    void DiffComponent(const char* name,
                       const std::map<Entity, T>& a,
                       const std::map<Entity, T>& b,
                       std::vector<std::string>& diffs)
    {
        if (a.size() != b.size())
        {
            diffs.push_back(std::string(name) + ": count " + std::to_string(a.size()) + " vs " +
                            std::to_string(b.size()));
        }
        for (const auto& kv : a)
        {
            auto it = b.find(kv.first);
            if (it == b.end())
                diffs.push_back(std::string(name) + ": missing on entity " + std::to_string(kv.first));
            else if (!Same(kv.second, it->second))
                diffs.push_back(std::string(name) + ": differs on entity " + std::to_string(kv.first));
        }
    }

    inline std::vector<std::string> Diff(const WorldImage& a, const WorldImage& b)
    {
        std::vector<std::string> diffs;
        if (a.Living != b.Living)
            diffs.push_back("living entity list differs");
        if (a.Available != b.Available)
            diffs.push_back("free entity queue differs");
        DiffComponent("Transform", a.Transforms, b.Transforms, diffs);
        DiffComponent("RigidBody", a.Bodies, b.Bodies, diffs);
        DiffComponent("SceneObject", a.Objects, b.Objects, diffs);
        DiffComponent("Shape2D", a.Shapes, b.Shapes, diffs);
        DiffComponent("ScriptComponent", a.Scripts, b.Scripts, diffs);
        DiffComponent("Mesh", a.Meshes, b.Meshes, diffs);
        DiffComponent("FragShaderTag", a.Shaders, b.Shaders, diffs);
        if (!Same(a.Settings, b.Settings))
            diffs.push_back("SceneSettings differ");
        return diffs;
    }

    inline std::string Describe(const std::vector<std::string>& diffs)
    {
        std::ostringstream out;
        for (const auto& d : diffs)
            out << "\n        - " << d;
        return out.str();
    }

#define CHECK_SAME_WORLD(a, b)                                                                 \
    do                                                                                         \
    {                                                                                          \
        ++TestFramework::TotalChecks();                                                        \
        auto tfDiffs = Fixture::Diff((a), (b));                                                \
        if (!tfDiffs.empty())                                                                  \
            TestFramework::ReportFailure(__FILE__, __LINE__,                                   \
                                         "worlds differ:" + Fixture::Describe(tfDiffs));       \
    } while (0)

    //-----------------------------------------------------------------------------
    // World setup
    //-----------------------------------------------------------------------------

    /**
     * \brief Empty world in the ScenePlayer (resets ECS, scripts, input)
     */
    inline void FreshWorld()
    {
        TestEnvironment::Init();
        GameSceneManager.SetActiveScene(ScenePlayer::NAME);
    }

    inline SceneObjects::ShapeDesc ShapeOf(const std::string& name,
                                           Shape2DType type,
                                           const Vec3& position,
                                           SceneObjects::BodyType body = SceneObjects::BodyType::None)
    {
        SceneObjects::ShapeDesc desc;
        desc.Name = name;
        desc.Shape.Type = type;
        desc.Position = position;
        desc.Body = body;
        return desc;
    }

    struct SampleWorld
    {
        Entity Field = NULL_ENTITY;
        Entity Player = NULL_ENTITY;
        Entity Wall = NULL_ENTITY;
        Entity Pickup = NULL_ENTITY;
        Entity Spinner = NULL_ENTITY;
        Entity Crate = NULL_ENTITY;
        Entity Parent = NULL_ENTITY;
        Entity Child = NULL_ENTITY;
        std::vector<Entity> Destroyed;
    };

    /**
     * \brief A scene using every feature of the scene format
     */
    inline SampleWorld BuildSampleWorld()
    {
        FreshWorld();
        SampleWorld w;
        using SceneObjects::BodyType;

        SceneObjects::ShapeDesc field = ShapeOf("Field", Shape2DType::Rectangle, {0, -0.05f, 0});
        field.Tag = "Field";
        field.Shape.Width = 30.0f;
        field.Shape.Height = 24.0f;
        field.Shape.Thickness = 0.05f;
        w.Field = SceneObjects::CreateShape(field);

        SceneObjects::ShapeDesc player = ShapeOf("Player", Shape2DType::Circle, {1, 0, -2}, BodyType::Dynamic);
        player.Tag = "Player";
        player.Shape.Width = 1.2f;
        player.Shape.Color = Vec3(0.3f, 0.5f, 0.9f);
        player.Script = "PlayerController";
        player.ScriptParams = {{"Speed", 7.5f}};
        w.Player = SceneObjects::CreateShape(player);
        ECS.GetComponent<RigidBody>(w.Player).Velocity = Vec2(0.5f, -0.25f);
        ECS.GetComponent<RigidBody>(w.Player).AngularVelocity = 0.125f;

        SceneObjects::ShapeDesc wall = ShapeOf("Wall", Shape2DType::Rectangle, {-6, 0, 3}, BodyType::Static);
        wall.Tag = "Wall";
        wall.Shape.Width = 8.0f;
        wall.Shape.Height = 0.75f;
        wall.YawDegrees = 30.0f;
        w.Wall = SceneObjects::CreateShape(wall);

        SceneObjects::ShapeDesc pickup = ShapeOf("Gem", Shape2DType::Triangle, {5, 0, 5}, BodyType::Trigger);
        pickup.Tag = "Pickup";
        pickup.Shape.Height = 1.5f;
        pickup.Shape.Color = Vec3(0.95f, 0.85f, 0.3f);
        pickup.Script = "Collectible";
        w.Pickup = SceneObjects::CreateShape(pickup);

        SceneObjects::ShapeDesc spinner = ShapeOf("Spinner", Shape2DType::Polygon, {-4, 0, -5}, BodyType::Static);
        spinner.Shape.Sides = 7;
        spinner.Shape.Width = 2.0f;
        spinner.Shape.Thickness = 0.6f;
        spinner.Script = "Rotator";
        spinner.ScriptParams = {{"Speed", -45.0f}};
        w.Spinner = SceneObjects::CreateShape(spinner);

        w.Crate = SceneObjects::CreateModel("Crate", "Box", {8, 0, -6}, 45.0f, 1.5f);
        SceneObjects::SetBodyType(w.Crate, BodyType::Dynamic);

        // Parent / child: the child follows the parent's transform
        w.Parent = SceneObjects::CreateShape(ShapeOf("Base", Shape2DType::Rectangle, {3, 0, 8}));
        w.Child = SceneObjects::CreateShape(ShapeOf("Turret", Shape2DType::Circle, {0, 0.3f, 0}));
        ECS.GetComponent<Transform>(w.Child).SetParentEntity(w.Parent, w.Child);

        // Objects removed while playing leave holes in the id space
        Entity gone1 = SceneObjects::CreateShape(ShapeOf("Gone 1", Shape2DType::Circle, {0, 0, 0}));
        Entity gone2 = SceneObjects::CreateShape(ShapeOf("Gone 2", Shape2DType::Circle, {1, 0, 0}));
        SceneObjects::Destroy(gone1);
        SceneObjects::Destroy(gone2);
        w.Destroyed = {gone1, gone2};
        ECS.FlushECS();

        auto settings = ECS.GetResource<SceneSettings>();
        settings->Name = "sample";
        settings->SceneScript = "CollectGame";
        settings->SceneParams = {{"Level", 4.0f}, {"Lives", 2.0f}};
        settings->FieldWidth = 30.0f;
        settings->FieldHeight = 24.0f;
        settings->CameraTarget = Vec3(1.0f, 0.0f, -2.0f);
        settings->CameraDistance = 22.5f;
        return w;
    }

    inline const Serialization::SerializationRegistry& Registry()
    {
        return Serialization::GetSceneSerializationRegistry();
    }

    inline Serialization::WorldSerializer Serializer()
    {
        return Serialization::WorldSerializer(Registry());
    }
} // namespace Fixture
