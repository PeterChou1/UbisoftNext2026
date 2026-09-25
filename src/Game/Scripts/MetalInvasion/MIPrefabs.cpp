#include "MIPrefabs.h"

#include "ECSManager.h"
#include "MINames.h"
#include "Map.h"
#include "World/SceneComponents.h"
#include "World/SceneObjects.h"

#include <cmath>
#include <map>
#include <string>

extern ECSManager ECS;

namespace MI
{
    namespace
    {
        constexpr float RAD_TO_DEG = 180.0f / 3.14159265f;

        // Runtime names only have to be readable, a counter keeps them unique
        // without searching the world
        std::string NextName(const std::string& prefix)
        {
            static int counter = 0;
            return prefix + " " + std::to_string(++counter);
        }

        struct ModelDesc
        {
            std::string Name;
            std::string Model;
            std::string Tag;
            std::string Script;
            std::map<std::string, float> Params;
            Vec3 Position = {0, 0, 0};
            float Yaw = 0.0f;
            float Scale = 1.0f;
            SceneObjects::BodyType Body = SceneObjects::BodyType::None;
        };

        Entity SpawnModel(const ModelDesc& desc)
        {
            Entity e = SceneObjects::CreateModel(desc.Name, desc.Model, desc.Position, desc.Yaw, desc.Scale);
            ECS.GetComponent<SceneObject>(e).Tag = desc.Tag;
            SceneObjects::SetBodyType(e, desc.Body);
            if (!desc.Script.empty())
                ECS.AddComponent<ScriptComponent>(e, {desc.Script, desc.Params});
            return e;
        }

        template <typename SpawnOne>
        void Formation(const Vec3& centre, int count, float spacing, SpawnOne spawn)
        {
            int perRow = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
            for (int i = 0; i < count; ++i)
            {
                float x = (i % perRow - (perRow - 1) * 0.5f) * spacing;
                float z = (i / perRow - (perRow - 1) * 0.5f) * spacing;
                spawn(centre + Vec3(x, 0.0f, z));
            }
        }

        Entity PlayerUnit(const char* name, const char* model, const char* script, float scale,
                          const Vec3& position, int battalion, float health)
        {
            ModelDesc desc;
            desc.Name = NextName(name);
            desc.Model = model;
            desc.Tag = Tags::Unit;
            desc.Script = script;
            desc.Params = {{"Battalion", static_cast<float>(battalion)}, {"Health", health}};
            desc.Position = position;
            desc.Scale = scale;
            desc.Body = SceneObjects::BodyType::Dynamic;
            return SpawnModel(desc);
        }
    } // namespace

    Side SideOfTag(const std::string& tag)
    {
        if (tag == Tags::Unit || tag == Tags::Wall || tag == Tags::Base)
            return Side::Player;
        if (tag == Tags::Enemy)
            return Side::Enemy;
        return Side::Neutral;
    }

    Side SideOf(Entity entity)
    {
        if (entity == NULL_ENTITY || !ECS.IsEntityAlive(entity) || !ECS.HasComponent<SceneObject>(entity))
            return Side::Neutral;
        return SideOfTag(ECS.GetComponent<SceneObject>(entity).Tag);
    }

    Entity SpawnSoldier(const Vec3& position, int battalion)
    {
        return PlayerUnit("Soldier", Models::Soldier, Scripts::Soldier, SOLDIER_SCALE, position, battalion, 100.0f);
    }

    Entity SpawnSupport(const Vec3& position, int battalion)
    {
        return PlayerUnit("Support", Models::Support, Scripts::Support, SUPPORT_SCALE, position, battalion, 100.0f);
    }

    Entity SpawnTank(const Vec3& position, int battalion)
    {
        return PlayerUnit("Tank", Models::TankHull, Scripts::Tank, TANK_SCALE, position, battalion, 200.0f);
    }

    void SpawnBattalion(const Vec3& position, int count, int battalion, bool support)
    {
        Formation(position, count, 0.6f, [&](const Vec3& p) {
            if (support)
                SpawnSupport(p, battalion);
            else
                SpawnSoldier(p, battalion);
        });
    }

    Entity SpawnEnemySoldier(const Vec3& position, float speed, float health)
    {
        ModelDesc desc;
        desc.Name = NextName("Enemy");
        desc.Model = Models::Enemy;
        desc.Tag = Tags::Enemy;
        desc.Script = Scripts::EnemySoldier;
        desc.Params = {{"Speed", speed}, {"Health", health}};
        desc.Position = position;
        desc.Scale = SOLDIER_SCALE;
        desc.Body = SceneObjects::BodyType::Dynamic;
        return SpawnModel(desc);
    }

    Entity SpawnEnemyTank(const Vec3& position, float health)
    {
        ModelDesc desc;
        desc.Name = NextName("Enemy tank");
        desc.Model = Models::TankHull;
        desc.Tag = Tags::Enemy;
        desc.Script = Scripts::EnemyTank;
        desc.Params = {{"Health", health}};
        desc.Position = position;
        desc.Scale = TANK_SCALE;
        desc.Body = SceneObjects::BodyType::Dynamic;
        return SpawnModel(desc);
    }

    void SpawnEnemyBattalion(const Vec3& position, int count, float speed, float health)
    {
        Formation(position, count, 0.6f, [&](const Vec3& p) { SpawnEnemySoldier(p, speed, health); });
    }

    Entity SpawnCrystal(const Vec3& position, int amount)
    {
        ModelDesc desc;
        desc.Name = NextName("Crystal");
        desc.Model = Models::Crystal;
        desc.Tag = Tags::Crystal;
        desc.Script = Scripts::Crystal;
        desc.Params = {{"Amount", static_cast<float>(amount)}};
        desc.Position = position;
        desc.Scale = CRYSTAL_SCALE;
        Entity crystal = SpawnModel(desc);
        // Blocks the path finding grid (engine component, saved with the scene)
        ECS.AddComponent<AIObstacle>(crystal, {0.6f, 0.6f});
        return crystal;
    }

    Entity SpawnWall(const Vec3& position, bool rotated, bool ghost)
    {
        SceneObjects::ShapeDesc desc;
        desc.Name = NextName(ghost ? "Wall preview" : "Wall");
        desc.Shape.Type = Shape2DType::Rectangle;
        desc.Shape.Width = WALL_WIDTH;
        desc.Shape.Height = WALL_LENGTH;
        desc.Shape.Thickness = 1.0f;
        desc.Shape.Color = Vec3(0.6f, 0.6f, 0.65f);
        desc.Position = position;
        desc.YawDegrees = rotated ? 90.0f : 0.0f;
        Entity wall = SceneObjects::CreateShape(desc);
        if (!ghost)
            BuildWall(wall, rotated);
        return wall;
    }

    void BuildWall(Entity wall, bool rotated)
    {
        ECS.GetComponent<SceneObject>(wall).Tag = Tags::Wall;
        ECS.GetComponent<SceneObject>(wall).Name = NextName("Wall");
        SceneObjects::SetBodyType(wall, SceneObjects::BodyType::Static);
        // Half extents on the grid, a little wider than the wall itself
        AIObstacle footprint{WALL_WIDTH * 0.5f + 0.3f, WALL_LENGTH * 0.5f + 0.3f};
        if (rotated)
            std::swap(footprint.Width, footprint.Height);
        ECS.AddComponent<AIObstacle>(wall, footprint);
        ECS.AddComponent<ScriptComponent>(wall, {Scripts::Wall, {{"Health", 250.0f}}});
    }

    Entity SpawnLaser(const Vec3& from, const Vec3& to, const Vec3& color)
    {
        Vec3 d = to - from;
        d.Y = 0.0f;
        SceneObjects::ShapeDesc desc;
        desc.Name = NextName("Laser");
        desc.Shape.Type = Shape2DType::Rectangle;
        desc.Shape.Width = 0.06f;
        desc.Shape.Height = std::max(d.GetMagnitude(), 0.1f);
        desc.Shape.Thickness = 0.06f;
        desc.Shape.Color = color;
        // Half way, a little above the ground
        desc.Position = Vec3((from.X + to.X) * 0.5f, 0.3f, (from.Z + to.Z) * 0.5f);
        desc.YawDegrees = std::atan2(d.X, d.Z) * RAD_TO_DEG;
        // The generic Mover script with no speed is a timed effect
        desc.Script = "Mover";
        desc.ScriptParams = {{"Speed", 0.0f}, {"Lifetime", 0.2f}};
        return SceneObjects::CreateShape(desc);
    }

    Entity SpawnBullet(const Vec3& position, float yawDegrees, Side side)
    {
        ModelDesc desc;
        desc.Name = NextName("Bullet");
        desc.Model = Models::Bullet;
        desc.Script = Scripts::Bullet;
        desc.Params = {{"Enemy", side == Side::Enemy ? 1.0f : 0.0f}};
        desc.Position = Vec3(position.X, 0.3f, position.Z);
        desc.Yaw = yawDegrees;
        desc.Scale = 0.3f;
        desc.Body = SceneObjects::BodyType::Trigger;
        return SpawnModel(desc);
    }

    Entity SpawnExplosion(const Vec3& position, Side side)
    {
        ModelDesc desc;
        desc.Name = NextName("Explosion");
        desc.Model = Models::Explosion;
        desc.Script = Scripts::Explosion;
        // 1 = hurts the player's side, 0 = hurts enemies, -1 = only visual
        desc.Params = {{"Enemy", side == Side::Enemy ? 1.0f : side == Side::Player ? 0.0f : -1.0f}};
        desc.Position = Vec3(position.X, 0.0f, position.Z);
        desc.Scale = 0.6f;
        return SpawnModel(desc);
    }
} // namespace MI
