#include "SceneEditor.h"

#include "../BasicEnemyUnit.h"
#include "../BlackBoard.h"
#include "../Crystal.h"
#include "../ECSManager.h"
#include "../GameState.h"
#include "../Mesh.h"
#include "../PlayerBase.h"
#include "../PlayerUnits.h"
#include "../Prefabs.h"
#include "../RigidBody.h"
#include "../Serialization/GameSerialization.h"
#include "../UIState.h"
#include "../UITarget.h"

#include <algorithm>
#include <cmath>

extern ECSManager ECS;

namespace Editor
{
    namespace
    {
        constexpr float PI_F = 3.14159265f;
        constexpr float DEG_TO_RAD = PI_F / 180.0f;
        constexpr float RAD_TO_DEG = 180.0f / PI_F;
        // Extra distance around an object that still counts as a hit
        constexpr float PICK_MARGIN = 0.15f;
        // Pick radius for objects without a physics body (crystals, selector)
        constexpr float DEFAULT_PICK_RADIUS = 0.6f;
        // Keep objects fully inside the vector fields
        constexpr float PLAY_AREA_MARGIN = 1.0f;

        const Vec3 BASE_POSITION = {0, 0, 0};
        const Vec3 SELECTOR_POSITION = {2, 2, 0};

        Serialization::WorldSerializer Serializer()
        {
            return Serialization::WorldSerializer(Serialization::GetGameSerializationRegistry());
        }

        bool HasMesh(Entity e, ObjAsset asset)
        {
            return ECS.HasComponent<Mesh>(e) && ECS.GetComponent<Mesh>(e).MeshType == asset;
        }

        void DestroyWithChildren(Entity e)
        {
            if (!ECS.IsEntityAlive(e))
                return;
            if (ECS.HasComponent<Transform>(e))
            {
                std::vector<Entity> children = ECS.GetComponent<Transform>(e).Children;
                for (Entity child : children)
                    DestroyWithChildren(child);
            }
            ECS.DestroyEntity(e);
        }

        bool HasHealth(EntityKind kind)
        {
            switch (kind)
            {
            case EntityKind::PlayerBase:
            case EntityKind::Soldier:
            case EntityKind::Support:
            case EntityKind::PlayerTank:
            case EntityKind::EnemySoldier:
            case EntityKind::EnemyTank:
            case EntityKind::Wall:
                return true;
            default:
                return false;
            }
        }

        bool IsWallFlipped(float degrees)
        {
            float normalized = std::fmod(std::fabs(degrees) + 45.0f, 180.0f);
            return normalized >= 90.0f;
        }
    } // namespace

    const char* PrefabName(PrefabType type)
    {
        switch (type)
        {
        case PrefabType::Soldier:
            return "Soldier";
        case PrefabType::Support:
            return "Support";
        case PrefabType::PlayerTank:
            return "Player Tank";
        case PrefabType::EnemySoldier:
            return "Enemy";
        case PrefabType::EnemyTank:
            return "Enemy Tank";
        case PrefabType::Crystal:
            return "Crystal";
        case PrefabType::Wall:
            return "Wall";
        default:
            return "?";
        }
    }

    const char* KindName(EntityKind kind)
    {
        switch (kind)
        {
        case EntityKind::Ground:
            return "Ground";
        case EntityKind::PlayerBase:
            return "Player Base";
        case EntityKind::Selector:
            return "Unit Selector";
        case EntityKind::Soldier:
            return "Soldier";
        case EntityKind::Support:
            return "Support";
        case EntityKind::PlayerTank:
            return "Player Tank";
        case EntityKind::EnemySoldier:
            return "Enemy";
        case EntityKind::EnemyTank:
            return "Enemy Tank";
        case EntityKind::Crystal:
            return "Crystal";
        case EntityKind::Wall:
            return "Wall";
        case EntityKind::Other:
            return "Other";
        default:
            return "None";
        }
    }

    //-----------------------------------------------------------------------------
    // Scene lifetime
    //-----------------------------------------------------------------------------

    void SceneEditor::NewScene()
    {
        ECS.ClearWorld();

        // Same round state as a freshly started main level
        auto state = ECS.GetResource<GameState>();
        *state = GameState{};
        state->ResetResource();
        state->CurCameraState = StartMenu;
        state->CameraEndState = StartMenu;

        // Everything the save system stores for the BlackBoard must be reset,
        // otherwise values from a previous game leak into the new scene
        auto board = ECS.GetResource<BlackBoard>();
        board->ResetResource();
        board->EnemyTankTargets.clear();
        board->EnemyTarget = NULL_ENTITY;
        board->DeltaTime = 0.0f;
        for (VectorField* field : {&board->UnitVectorField, &board->EnemyVectorField})
        {
            *field = VectorField{};
            field->LocationVectorField = Vec3(0, 0, 0);
            field->HalfWidth = Prefabs::PLAY_AREA_HALF_SIZE;
            field->HalfHeight = Prefabs::PLAY_AREA_HALF_SIZE;
        }

        auto ui = ECS.GetResource<UIState>();
        ui->state = DefaultContext;
        ui->flipped = false;

        Prefabs::SpawnGround();
        Prefabs::SpawnPlayerBase(BASE_POSITION);
        board->UnitTarget = Prefabs::SpawnUnitSelector(SELECTOR_POSITION);

        ClearHistory();
        m_Selected = NULL_ENTITY;
        m_Dirty = false;
        if (OnWorldReplaced)
            OnWorldReplaced();
    }

    //-----------------------------------------------------------------------------
    // Entities
    //-----------------------------------------------------------------------------

    Entity SceneEditor::Place(PrefabType type, const Vec3& position, const PrefabSettings& settings)
    {
        if (type >= PrefabType::Count)
            return NULL_ENTITY;
        RecordUndo();
        Vec3 p = ClampToPlayArea(position);
        Entity e = NULL_ENTITY;
        switch (type)
        {
        case PrefabType::Soldier:
            e = Prefabs::SpawnSoldier(p, settings.Health, settings.Battalion);
            break;
        case PrefabType::Support:
            e = Prefabs::SpawnSupport(p, settings.Health, settings.Battalion);
            break;
        case PrefabType::PlayerTank:
            e = Prefabs::SpawnPlayerTank(p, settings.Battalion, settings.Health);
            break;
        case PrefabType::EnemySoldier:
            e = Prefabs::SpawnEnemySoldier(p, settings.Health, settings.EnemySpeed);
            break;
        case PrefabType::EnemyTank:
            e = Prefabs::SpawnEnemyTank(p, settings.Health);
            break;
        case PrefabType::Crystal:
            e = Prefabs::SpawnCrystal(p, settings.CrystalAmount);
            break;
        case PrefabType::Wall:
            e = Prefabs::SpawnWall(p, IsWallFlipped(settings.YawDegrees));
            break;
        default:
            break;
        }
        if (type != PrefabType::Wall && settings.YawDegrees != 0.0f)
        {
            ECS.GetComponent<Transform>(e).SetGlobalRotation(
                    Quat({0, 1, 0}, settings.YawDegrees * DEG_TO_RAD));
        }
        MarkEdited();
        m_Selected = e;
        return e;
    }

    EntityKind SceneEditor::KindOf(Entity e) const
    {
        if (e == NULL_ENTITY || !ECS.IsEntityAlive(e))
            return EntityKind::None;
        if (ECS.HasComponent<UITarget>(e))
            return EntityKind::Selector;
        if (ECS.HasComponent<PlayerBaseComponent>(e))
            return EntityKind::PlayerBase;
        if (ECS.HasComponent<CrystalDeposit>(e))
            return EntityKind::Crystal;
        if (ECS.HasComponent<PlayerControlUnit>(e))
        {
            if (ECS.GetComponent<PlayerControlUnit>(e).isTank)
                return EntityKind::PlayerTank;
            if (HasMesh(e, SoldierUnitAsset))
                return EntityKind::Soldier;
            if (HasMesh(e, SupportUnitAsset))
                return EntityKind::Support;
            if (HasMesh(e, ObstacleWall))
                return EntityKind::Wall;
            return EntityKind::Other;
        }
        if (ECS.HasComponent<BasicEnemyUnit>(e))
        {
            if (HasMesh(e, BasicEnemy))
                return EntityKind::EnemySoldier;
            return EntityKind::EnemyTank;
        }
        if (HasMesh(e, Ground))
            return EntityKind::Ground;
        return EntityKind::Other;
    }

    bool SceneEditor::IsEditable(Entity e) const
    {
        EntityKind kind = KindOf(e);
        return kind != EntityKind::None && kind != EntityKind::Ground && kind != EntityKind::Other;
    }

    std::vector<Entity> SceneEditor::EditableEntities() const
    {
        std::vector<Entity> result;
        for (Entity e : ECS.GetLivingEntities())
        {
            if (IsEditable(e))
                result.push_back(e);
        }
        return result;
    }

    Entity SceneEditor::Pick(const Vec3& groundPoint) const
    {
        Entity best = NULL_ENTITY;
        float bestDistance = 1e30f;
        for (Entity e : EditableEntities())
        {
            Transform& t = ECS.GetComponent<Transform>(e);
            float dx = groundPoint.X - t.LocalPosition.X;
            float dz = groundPoint.Z - t.LocalPosition.Z;
            bool hit = false;

            const RigidBody* body =
                    ECS.HasComponent<RigidBody>(e) ? &ECS.GetComponent<RigidBody>(e) : nullptr;
            if (body != nullptr && body->Shape.GetShapeType() == PolygonShape)
            {
                // Oriented rectangle test in the object's local frame. The shape is
                // symmetric so the direction of rotation does not matter
                Vec2 axisX(t.Affine[0][0], t.Affine[2][0]);
                Vec2 axisZ(t.Affine[0][2], t.Affine[2][2]);
                axisX.Normalize();
                axisZ.Normalize();
                float localX = dx * axisX.X + dz * axisX.Y;
                float localZ = dx * axisZ.X + dz * axisZ.Y;
                hit = std::fabs(localX) <= body->Shape.Width * 0.5f + PICK_MARGIN &&
                      std::fabs(localZ) <= body->Shape.Height * 0.5f + PICK_MARGIN;
            }
            else
            {
                float radius = body != nullptr ? body->Shape.Radius : DEFAULT_PICK_RADIUS;
                hit = dx * dx + dz * dz <= (radius + PICK_MARGIN) * (radius + PICK_MARGIN);
            }

            float distance = dx * dx + dz * dz;
            if (hit && distance < bestDistance)
            {
                best = e;
                bestDistance = distance;
            }
        }
        return best;
    }

    Vec3 SceneEditor::GetPosition(Entity e) const
    {
        return ECS.GetComponent<Transform>(e).LocalPosition;
    }

    bool SceneEditor::Move(Entity e, const Vec3& position, bool recordUndo)
    {
        if (!IsEditable(e))
            return false;
        if (recordUndo)
            RecordUndo();
        Transform& t = ECS.GetComponent<Transform>(e);
        Vec3 target = ClampToPlayArea(position);
        // Objects stay at their original height (the selector floats above ground)
        target.Y = t.LocalPosition.Y;
        t.SetLocalPosition(target);
        MarkEdited();
        return true;
    }

    bool SceneEditor::SetYaw(Entity e, float degrees)
    {
        if (!IsEditable(e))
            return false;
        RecordUndo();
        if (KindOf(e) == EntityKind::Wall)
            Prefabs::SetWallFlipped(e, IsWallFlipped(degrees));
        else
            ECS.GetComponent<Transform>(e).SetGlobalRotation(Quat({0, 1, 0}, degrees * DEG_TO_RAD));
        MarkEdited();
        return true;
    }

    float SceneEditor::GetYaw(Entity e) const
    {
        if (!IsEditable(e))
            return 0.0f;
        float degrees = ECS.GetComponent<Transform>(e).LocalRotation.GetPitch2D() * RAD_TO_DEG;
        degrees = std::fmod(degrees, 360.0f);
        if (degrees < 0.0f)
            degrees += 360.0f;
        // Remove float noise such as 89.99998
        float rounded = std::round(degrees * 100.0f) / 100.0f;
        return rounded >= 360.0f ? 0.0f : rounded;
    }

    bool SceneEditor::Remove(Entity e)
    {
        EntityKind kind = KindOf(e);
        if (!IsEditable(e) || kind == EntityKind::PlayerBase || kind == EntityKind::Selector)
            return false;
        RecordUndo();
        // No FlushECS here: the render systems pick the deletion up next frame
        DestroyWithChildren(e);
        if (m_Selected == e)
            m_Selected = NULL_ENTITY;
        MarkEdited();
        return true;
    }

    //-----------------------------------------------------------------------------
    // Properties
    //-----------------------------------------------------------------------------

    int SceneEditor::GetHealth(Entity e) const
    {
        if (!HasHealth(KindOf(e)))
            return -1;
        switch (KindOf(e))
        {
        case EntityKind::Soldier:
        case EntityKind::Support:
        case EntityKind::PlayerTank:
        case EntityKind::Wall:
            return ECS.GetComponent<PlayerControlUnit>(e).health;
        case EntityKind::EnemySoldier:
        case EntityKind::EnemyTank:
            return ECS.GetComponent<BasicEnemyUnit>(e).health;
        case EntityKind::PlayerBase:
            return ECS.GetComponent<PlayerBaseComponent>(e).PlayerBaseHealth;
        default:
            return -1;
        }
    }

    bool SceneEditor::SetHealth(Entity e, int health)
    {
        EntityKind kind = KindOf(e);
        if (!HasHealth(kind))
            return false;
        RecordUndo();
        health = std::max(1, health);
        if (kind == EntityKind::PlayerBase)
            ECS.GetComponent<PlayerBaseComponent>(e).PlayerBaseHealth = health;
        else if (kind == EntityKind::EnemySoldier || kind == EntityKind::EnemyTank)
            ECS.GetComponent<BasicEnemyUnit>(e).health = health;
        else
            ECS.GetComponent<PlayerControlUnit>(e).health = health;
        MarkEdited();
        return true;
    }

    int SceneEditor::GetBattalion(Entity e) const
    {
        EntityKind kind = KindOf(e);
        if (kind == EntityKind::Soldier || kind == EntityKind::Support ||
            kind == EntityKind::PlayerTank)
            return ECS.GetComponent<PlayerControlUnit>(e).battalionId;
        return -1;
    }

    bool SceneEditor::SetBattalion(Entity e, int battalion)
    {
        if (GetBattalion(e) < 0 || battalion < 0)
            return false;
        RecordUndo();
        ECS.GetComponent<PlayerControlUnit>(e).battalionId = battalion;
        MarkEdited();
        return true;
    }

    int SceneEditor::GetCrystalAmount(Entity e) const
    {
        if (KindOf(e) != EntityKind::Crystal)
            return -1;
        return ECS.GetComponent<CrystalDeposit>(e).AmountOfCrystal;
    }

    bool SceneEditor::SetCrystalAmount(Entity e, int amount)
    {
        if (KindOf(e) != EntityKind::Crystal)
            return false;
        RecordUndo();
        // A deposit with 0 crystals is removed by the game immediately
        ECS.GetComponent<CrystalDeposit>(e).AmountOfCrystal = std::max(1, amount);
        MarkEdited();
        return true;
    }

    float SceneEditor::GetEnemySpeed(Entity e) const
    {
        if (KindOf(e) != EntityKind::EnemySoldier)
            return -1.0f;
        return ECS.GetComponent<BasicEnemyUnit>(e).Speed;
    }

    bool SceneEditor::SetEnemySpeed(Entity e, float speed)
    {
        if (KindOf(e) != EntityKind::EnemySoldier || speed <= 0.0f)
            return false;
        RecordUndo();
        ECS.GetComponent<BasicEnemyUnit>(e).Speed = speed;
        MarkEdited();
        return true;
    }

    //-----------------------------------------------------------------------------
    // Round settings
    //-----------------------------------------------------------------------------

    int SceneEditor::GetStartingCrystals() const
    {
        return ECS.GetResource<GameState>()->PlayerCrystalInventory;
    }

    void SceneEditor::SetStartingCrystals(int crystals)
    {
        RecordUndo();
        ECS.GetResource<GameState>()->PlayerCrystalInventory = std::max(0, crystals);
        MarkEdited();
    }

    int SceneEditor::GetRoundNumber() const
    {
        return ECS.GetResource<GameState>()->RoundNumber;
    }

    void SceneEditor::SetRoundNumber(int round)
    {
        RecordUndo();
        ECS.GetResource<GameState>()->RoundNumber = std::max(1, round);
        MarkEdited();
    }

    int SceneEditor::GetSpawnVolume() const
    {
        return ECS.GetResource<GameState>()->SpawnVolume;
    }

    void SceneEditor::SetSpawnVolume(int volume)
    {
        RecordUndo();
        ECS.GetResource<GameState>()->SpawnVolume = std::max(1, volume);
        MarkEdited();
    }

    //-----------------------------------------------------------------------------
    // Validation / files
    //-----------------------------------------------------------------------------

    std::vector<std::string> SceneEditor::Validate() const
    {
        std::vector<std::string> issues;
        size_t bases = 0;
        size_t selectors = 0;
        for (Entity e : ECS.GetLivingEntities())
        {
            EntityKind kind = KindOf(e);
            if (kind == EntityKind::PlayerBase)
                ++bases;
            if (kind == EntityKind::Selector)
                ++selectors;
            if (IsEditable(e))
            {
                Vec3 p = GetPosition(e);
                Vec3 clamped = ClampToPlayArea(p);
                if (clamped.X != p.X || clamped.Z != p.Z)
                    issues.push_back(std::string(KindName(kind)) + " " + std::to_string(e) +
                                     " is outside the play area");
            }
        }
        if (bases != 1)
            issues.push_back("Scene needs exactly one player base (found " + std::to_string(bases) +
                             ")");
        if (selectors != 1)
            issues.push_back("Scene needs exactly one unit selector (found " +
                             std::to_string(selectors) + ")");
        return issues;
    }

    std::vector<std::uint8_t> SceneEditor::SaveSceneToBytes(const std::string& name) const
    {
        return Serializer().Save(
                ECS,
                {{META_SCENE, PLAYABLE_SCENE}, {META_NAME, name}, {META_TOOL, "SceneEditor 1"}});
    }

    Serialization::SaveResult SceneEditor::SaveScene(const std::string& path,
                                                     const std::string& name)
    {
        Serialization::SaveResult result;
        std::vector<std::string> issues = Validate();
        if (!issues.empty())
        {
            result.Error = "Scene is not playable: " + issues.front();
            return result;
        }
        try
        {
            std::vector<std::uint8_t> bytes = SaveSceneToBytes(name);
            if (!Serialization::WorldSerializer::WriteFile(path, bytes, result.Error))
                return result;
            result.BytesWritten = bytes.size();
            result.Success = true;
            m_Dirty = false;
        }
        catch (const std::exception& e)
        {
            result.Error = e.what();
        }
        return result;
    }

    Serialization::LoadResult SceneEditor::LoadScene(const std::string& path)
    {
        Serialization::LoadResult result;
        std::vector<std::uint8_t> bytes;
        if (!Serialization::WorldSerializer::ReadFile(path, bytes, result.Error))
            return result;

        Serialization::WorldSnapshot snapshot;
        result = Serializer().Parse(bytes, snapshot);
        if (!result)
            return result;
        auto scene = result.Metadata.find(META_SCENE);
        if (scene == result.Metadata.end() || scene->second != PLAYABLE_SCENE)
        {
            result.Success = false;
            result.Error = "File is not a main level scene";
            return result;
        }

        std::vector<std::uint8_t> previous = Snapshot();
        std::vector<std::string> warnings = Serializer().Apply(ECS, snapshot);
        result.Warnings.insert(result.Warnings.end(), warnings.begin(), warnings.end());
        std::vector<std::string> issues = Validate();
        if (!issues.empty())
        {
            Restore(previous);
            result.Success = false;
            result.Error = "Scene is not playable: " + issues.front();
            return result;
        }

        ClearHistory();
        m_Selected = NULL_ENTITY;
        m_Dirty = false;
        if (OnWorldReplaced)
            OnWorldReplaced();
        return result;
    }

    //-----------------------------------------------------------------------------
    // Undo / redo
    //-----------------------------------------------------------------------------

    std::vector<std::uint8_t> SceneEditor::Snapshot() const
    {
        return Serializer().Save(ECS);
    }

    void SceneEditor::Restore(const std::vector<std::uint8_t>& snapshot)
    {
        Serialization::LoadResult result = Serializer().Load(ECS, snapshot);
        if (!result)
            throw Serialization::SerializationError("Undo snapshot failed to load: " +
                                                    result.Error);
        if (m_Selected != NULL_ENTITY && !IsEditable(m_Selected))
            m_Selected = NULL_ENTITY;
        if (OnWorldReplaced)
            OnWorldReplaced();
    }

    void SceneEditor::RecordUndo()
    {
        m_UndoStack.push_back(Snapshot());
        if (m_UndoStack.size() > MAX_UNDO)
            m_UndoStack.erase(m_UndoStack.begin());
        m_RedoStack.clear();
    }

    bool SceneEditor::Undo()
    {
        if (m_UndoStack.empty())
            return false;
        m_RedoStack.push_back(Snapshot());
        std::vector<std::uint8_t> snapshot = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();
        Restore(snapshot);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::Redo()
    {
        if (m_RedoStack.empty())
            return false;
        m_UndoStack.push_back(Snapshot());
        std::vector<std::uint8_t> snapshot = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        Restore(snapshot);
        m_Dirty = true;
        return true;
    }

    void SceneEditor::MarkEdited()
    {
        m_Dirty = true;
    }

    void SceneEditor::ClearHistory()
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
    }

    void SceneEditor::Select(Entity entity)
    {
        m_Selected = IsEditable(entity) ? entity : NULL_ENTITY;
    }

    //-----------------------------------------------------------------------------
    // Helpers
    //-----------------------------------------------------------------------------

    float SceneEditor::Snap(float value, float step)
    {
        if (step <= 0.0f)
            return value;
        return std::round(value / step) * step;
    }

    Vec3 SceneEditor::ClampToPlayArea(const Vec3& position)
    {
        float limit = Prefabs::PLAY_AREA_HALF_SIZE - PLAY_AREA_MARGIN;
        Vec3 clamped = position;
        clamped.X = std::max(-limit, std::min(limit, clamped.X));
        clamped.Z = std::max(-limit, std::min(limit, clamped.Z));
        return clamped;
    }
} // namespace Editor
