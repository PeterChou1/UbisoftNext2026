#include "SceneEditor.h"

#include "ECSManager.h"
#include "GameManager.h"
#include "Mesh.h"
#include "Scripting/ScriptRegistry.h"
#include "Serialization/SceneSerialization.h"
#include "Transform.h"
#include "World/SceneComponents.h"
#include "World/ShapeGeometry.h"

#include <algorithm>
#include <cmath>
#include <set>

extern ECSManager ECS;

using SceneObjects::BodyType;

namespace Editor
{
    namespace
    {
        constexpr float FIELD_THICKNESS = 0.05f;
        const Vec3 FIELD_COLOR = {0.32f, 0.38f, 0.34f};
        // Extra distance around an object that still counts as a hit
        constexpr float PICK_MARGIN = 0.1f;
        constexpr float MIN_SIZE = 0.1f;
        constexpr float MAX_SIZE = 200.0f;
        // Where a duplicate appears relative to the original
        const Vec3 DUPLICATE_OFFSET = {1.0f, 0.0f, -1.0f};

        Serialization::WorldSerializer Serializer()
        {
            return Serialization::WorldSerializer(Serialization::GetSceneSerializationRegistry());
        }

        float FootprintArea(Entity e)
        {
            if (ECS.HasComponent<Shape2D>(e))
            {
                const Shape2D& s = ECS.GetComponent<Shape2D>(e);
                bool round = s.Type == Shape2DType::Circle || s.Type == Shape2DType::Polygon;
                return s.Width * (round ? s.Width : s.Height);
            }
            float scale = ECS.GetComponent<Transform>(e).LocalScale.X;
            return scale * scale;
        }
    } // namespace

    const char* ObjectKindName(ObjectKind kind)
    {
        switch (kind)
        {
        case ObjectKind::Rectangle:
            return "Rectangle";
        case ObjectKind::Circle:
            return "Circle";
        case ObjectKind::Triangle:
            return "Triangle";
        case ObjectKind::Polygon:
            return "Polygon";
        case ObjectKind::Model:
            return "Model";
        default:
            return "?";
        }
    }

    //-----------------------------------------------------------------------------
    // Scene lifetime
    //-----------------------------------------------------------------------------

    void SceneEditor::NewScene()
    {
        ECS.ClearWorld();
        auto settings = ECS.GetResource<SceneSettings>();
        *settings = SceneSettings{};

        SceneObjects::ShapeDesc field;
        field.Name = FIELD_NAME;
        field.Tag = FIELD_NAME;
        field.Shape.Type = Shape2DType::Rectangle;
        field.Shape.Width = settings->FieldWidth;
        field.Shape.Height = settings->FieldHeight;
        field.Shape.Thickness = FIELD_THICKNESS;
        field.Shape.Color = FIELD_COLOR;
        // Top of the field at y = 0, objects stand on it
        field.Position = Vec3(0.0f, -FIELD_THICKNESS, 0.0f);
        SceneObjects::CreateShape(field);

        m_UndoStack.clear();
        m_RedoStack.clear();
        m_Selected = NULL_ENTITY;
        m_Dirty = false;
        m_Playing = false;
        WorldReplaced();
    }

    //-----------------------------------------------------------------------------
    // Objects
    //-----------------------------------------------------------------------------

    Entity SceneEditor::Place(ObjectKind kind, const Vec3& position, const PlaceSettings& settings)
    {
        if (kind >= ObjectKind::Count)
            return NULL_ENTITY;
        if (kind == ObjectKind::Model && settings.Model.empty())
            return NULL_ENTITY;
        RecordUndo();
        Vec3 p = ClampToField(position);
        p.Y = 0.0f;
        Entity e = NULL_ENTITY;
        if (kind == ObjectKind::Model)
        {
            e = SceneObjects::CreateModel(SceneObjects::UniqueName(settings.Model),
                                          settings.Model,
                                          p,
                                          settings.YawDegrees,
                                          std::max(settings.Width, MIN_SIZE));
            ECS.GetComponent<SceneObject>(e).Tag = settings.Tag;
            SceneObjects::SetBodyType(e, settings.Body);
        }
        else
        {
            SceneObjects::ShapeDesc desc;
            desc.Name = SceneObjects::UniqueName(ObjectKindName(kind));
            desc.Tag = settings.Tag;
            desc.Shape.Type = static_cast<Shape2DType>(kind);
            desc.Shape.Width = std::clamp(settings.Width, MIN_SIZE, MAX_SIZE);
            desc.Shape.Height = std::clamp(settings.Height, MIN_SIZE, MAX_SIZE);
            desc.Shape.Sides = std::clamp(
                    settings.Sides, ShapeGeometry::MIN_POLYGON_SIDES, ShapeGeometry::MAX_POLYGON_SIDES);
            desc.Shape.Thickness = std::max(settings.Thickness, 0.0f);
            desc.Shape.Color = settings.Color;
            desc.Position = p;
            desc.YawDegrees = settings.YawDegrees;
            desc.Body = settings.Body;
            e = SceneObjects::CreateShape(desc);
        }
        m_Dirty = true;
        m_Selected = e;
        return e;
    }

    Entity SceneEditor::Duplicate(Entity source)
    {
        if (!CanEdit(source))
            return NULL_ENTITY;
        RecordUndo();
        Vec3 position = ClampToField(SceneObjects::GetPosition(source) + DUPLICATE_OFFSET);
        const SceneObject& object = ECS.GetComponent<SceneObject>(source);
        Entity copy = NULL_ENTITY;
        if (ECS.HasComponent<Shape2D>(source))
        {
            SceneObjects::ShapeDesc desc;
            desc.Name = SceneObjects::UniqueName(object.Name);
            desc.Tag = object.Tag;
            desc.Shape = ECS.GetComponent<Shape2D>(source);
            desc.Position = position;
            desc.YawDegrees = SceneObjects::GetYaw(source);
            desc.Body = SceneObjects::GetBodyType(source);
            copy = SceneObjects::CreateShape(desc);
        }
        else
        {
            copy = SceneObjects::CreateModel(SceneObjects::UniqueName(object.Name),
                                             ECS.GetComponent<Mesh>(source).Model,
                                             position,
                                             SceneObjects::GetYaw(source),
                                             ECS.GetComponent<Transform>(source).LocalScale.X);
            ECS.GetComponent<SceneObject>(copy).Tag = object.Tag;
            SceneObjects::SetBodyType(copy, SceneObjects::GetBodyType(source));
        }
        if (ECS.HasComponent<ScriptComponent>(source))
            ECS.AddComponent<ScriptComponent>(copy, ECS.GetComponent<ScriptComponent>(source));
        m_Dirty = true;
        m_Selected = copy;
        return copy;
    }

    std::vector<Entity> SceneEditor::Objects() const
    {
        std::vector<Entity> result;
        for (Entity e : ECS.GetLivingEntities())
        {
            if (ECS.HasComponent<SceneObject>(e))
                result.push_back(e);
        }
        return result;
    }

    bool SceneEditor::IsObject(Entity entity) const
    {
        return entity != NULL_ENTITY && ECS.IsEntityAlive(entity) &&
               ECS.HasComponent<SceneObject>(entity);
    }

    bool SceneEditor::IsField(Entity entity) const
    {
        return IsObject(entity) && ECS.GetComponent<SceneObject>(entity).Tag == FIELD_NAME;
    }

    bool SceneEditor::CanEdit(Entity entity) const
    {
        return IsObject(entity) && !IsField(entity);
    }

    ObjectKind SceneEditor::KindOf(Entity entity) const
    {
        if (IsObject(entity) && ECS.HasComponent<Shape2D>(entity))
            return static_cast<ObjectKind>(ECS.GetComponent<Shape2D>(entity).Type);
        return ObjectKind::Model;
    }

    std::string SceneEditor::NameOf(Entity entity) const
    {
        return IsObject(entity) ? ECS.GetComponent<SceneObject>(entity).Name : std::string();
    }

    Entity SceneEditor::Pick(const Vec3& groundPoint) const
    {
        Entity best = NULL_ENTITY;
        float bestArea = 1e30f;
        Entity field = NULL_ENTITY;
        for (Entity e : Objects())
        {
            if (!SceneObjects::Contains(e, groundPoint, PICK_MARGIN))
                continue;
            if (IsField(e))
            {
                field = e;
                continue;
            }
            float area = FootprintArea(e);
            if (area < bestArea)
            {
                best = e;
                bestArea = area;
            }
        }
        return best != NULL_ENTITY ? best : field;
    }

    Entity SceneEditor::PickRay(const std::function<Vec3(float)>& pointAtHeight, float* hitHeight) const
    {
        constexpr int SAMPLES = 8;
        Entity best = NULL_ENTITY;
        float bestArea = 1e30f;
        float bestHeight = 0.0f;
        Entity field = NULL_ENTITY;
        for (Entity e : Objects())
        {
            // Where does the ray go through the object's body? Sampled from
            // the top down: the first hit is the surface the user sees
            float base = SceneObjects::GetPosition(e).Y;
            float height = ECS.HasComponent<Shape2D>(e) ? ECS.GetComponent<Shape2D>(e).Thickness
                                                         : ECS.GetComponent<Transform>(e).LocalScale.Y;
            height = std::max(height, 0.0f);
            bool hit = false;
            float at = base;
            for (int i = SAMPLES; i >= 0 && !hit; --i)
            {
                at = base + height * static_cast<float>(i) / SAMPLES;
                Vec3 point = pointAtHeight(at);
                hit = point.IsValid() && SceneObjects::Contains(e, point, PICK_MARGIN);
            }
            if (!hit)
                continue;
            if (IsField(e))
            {
                field = e;
                continue;
            }
            float area = FootprintArea(e);
            if (area < bestArea)
            {
                best = e;
                bestArea = area;
                bestHeight = at;
            }
        }
        if (hitHeight != nullptr)
            *hitHeight = best != NULL_ENTITY ? bestHeight : 0.0f;
        return best != NULL_ENTITY ? best : field;
    }

    bool SceneEditor::Move(Entity entity, const Vec3& position, bool recordUndo)
    {
        if (!CanEdit(entity))
            return false;
        if (recordUndo)
            RecordUndo();
        Vec3 target = ClampToField(position);
        target.Y = SceneObjects::GetPosition(entity).Y;
        SceneObjects::SetPosition(entity, target);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetYaw(Entity entity, float degrees)
    {
        if (!CanEdit(entity))
            return false;
        RecordUndo();
        SceneObjects::SetYaw(entity, degrees);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::Remove(Entity entity)
    {
        if (!CanEdit(entity))
            return false;
        RecordUndo();
        // No FlushECS: the render systems pick the deletion up next frame
        SceneObjects::Destroy(entity);
        if (m_Selected == entity)
            m_Selected = NULL_ENTITY;
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetSize(Entity entity, float width, float height)
    {
        if (!IsObject(entity))
            return false;
        width = std::clamp(width, MIN_SIZE, MAX_SIZE);
        height = std::clamp(height, MIN_SIZE, MAX_SIZE);
        if (IsField(entity))
        {
            SetFieldSize(width, height);
            return true;
        }
        RecordUndo();
        if (ECS.HasComponent<Shape2D>(entity))
        {
            Shape2D& shape = ECS.GetComponent<Shape2D>(entity);
            shape.Width = width;
            shape.Height = height;
        }
        else
        {
            // Models scale uniformly
            Transform& t = ECS.GetComponent<Transform>(entity);
            t.Scale(width / std::max(t.LocalScale.X, 0.0001f));
        }
        SceneObjects::ShapeChanged(entity);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetSides(Entity entity, int sides)
    {
        if (!CanEdit(entity) || KindOf(entity) != ObjectKind::Polygon)
            return false;
        RecordUndo();
        ECS.GetComponent<Shape2D>(entity).Sides =
                std::clamp(sides, ShapeGeometry::MIN_POLYGON_SIDES, ShapeGeometry::MAX_POLYGON_SIDES);
        SceneObjects::ShapeChanged(entity);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetThickness(Entity entity, float thickness)
    {
        if (!CanEdit(entity) || !ECS.HasComponent<Shape2D>(entity))
            return false;
        RecordUndo();
        ECS.GetComponent<Shape2D>(entity).Thickness = std::clamp(thickness, 0.0f, 10.0f);
        SceneObjects::ShapeChanged(entity);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetColor(Entity entity, const Vec3& color)
    {
        if (!IsObject(entity) || !ECS.HasComponent<Shape2D>(entity))
            return false;
        RecordUndo();
        ECS.GetComponent<Shape2D>(entity).Color = color;
        ECS.GetComponent<Shape2D>(entity).Built = false;
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetBody(Entity entity, BodyType body)
    {
        if (!CanEdit(entity) || body >= BodyType::Count)
            return false;
        RecordUndo();
        SceneObjects::SetBodyType(entity, body);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetTag(Entity entity, const std::string& tag)
    {
        // The field tag identifies the field, it can not be given to objects
        if (!CanEdit(entity) || tag == FIELD_NAME)
            return false;
        RecordUndo();
        ECS.GetComponent<SceneObject>(entity).Tag = tag;
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::Rename(Entity entity, const std::string& name)
    {
        if (!CanEdit(entity) || name.empty() || name == FIELD_NAME)
            return false;
        if (ECS.GetComponent<SceneObject>(entity).Name == name)
            return true;
        Entity existing = SceneObjects::FindByName(name);
        if (existing != NULL_ENTITY && existing != entity)
            return false;
        RecordUndo();
        ECS.GetComponent<SceneObject>(entity).Name = name;
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetModel(Entity entity, const std::string& model)
    {
        if (!CanEdit(entity) || !ECS.HasComponent<Mesh>(entity) || model.empty())
            return false;
        RecordUndo();
        // The MeshHandler replaces the geometry of a mesh that is not Loaded
        Mesh& mesh = ECS.GetComponent<Mesh>(entity);
        mesh.Model = model;
        mesh.Loaded = false;
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetScript(Entity entity, const std::string& script)
    {
        if (!CanEdit(entity))
            return false;
        if (!script.empty())
        {
            const ScriptInfo* info = ScriptRegistry::Get().Find(script);
            if (info == nullptr || info->IsSceneScript)
                return false;
        }
        RecordUndo();
        if (ECS.HasComponent<ScriptComponent>(entity))
            ECS.RemoveComponent<ScriptComponent>(entity);
        if (!script.empty())
            ECS.AddComponent<ScriptComponent>(entity, {script, {}});
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetScriptParam(Entity entity, const std::string& param, float value)
    {
        if (!CanEdit(entity) || !ECS.HasComponent<ScriptComponent>(entity))
            return false;
        ScriptComponent& component = ECS.GetComponent<ScriptComponent>(entity);
        const ScriptInfo* info = ScriptRegistry::Get().Find(component.Script);
        if (info == nullptr ||
            std::none_of(info->Params.begin(), info->Params.end(), [&](const ScriptParam& p) {
                return p.Name == param;
            }))
            return false;
        RecordUndo();
        ECS.GetComponent<ScriptComponent>(entity).Params[param] = value;
        m_Dirty = true;
        return true;
    }

    std::string SceneEditor::GetScript(Entity entity) const
    {
        if (!IsObject(entity) || !ECS.HasComponent<ScriptComponent>(entity))
            return {};
        return ECS.GetComponent<ScriptComponent>(entity).Script;
    }

    float SceneEditor::GetScriptParam(Entity entity, const std::string& param) const
    {
        std::string script = GetScript(entity);
        if (script.empty())
            return 0.0f;
        auto params = ScriptRegistry::Get().ResolveParams(
                script, ECS.GetComponent<ScriptComponent>(entity).Params);
        auto it = params.find(param);
        return it == params.end() ? 0.0f : it->second;
    }

    //-----------------------------------------------------------------------------
    // Scene settings
    //-----------------------------------------------------------------------------

    bool SceneEditor::SetSceneScript(const std::string& script)
    {
        if (!script.empty())
        {
            const ScriptInfo* info = ScriptRegistry::Get().Find(script);
            if (info == nullptr || !info->IsSceneScript)
                return false;
        }
        RecordUndo();
        auto settings = ECS.GetResource<SceneSettings>();
        settings->SceneScript = script;
        settings->SceneParams.clear();
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetSceneParam(const std::string& param, float value)
    {
        auto settings = ECS.GetResource<SceneSettings>();
        const ScriptInfo* info = ScriptRegistry::Get().Find(settings->SceneScript);
        if (info == nullptr ||
            std::none_of(info->Params.begin(), info->Params.end(), [&](const ScriptParam& p) {
                return p.Name == param;
            }))
            return false;
        RecordUndo();
        settings->SceneParams[param] = value;
        m_Dirty = true;
        return true;
    }

    float SceneEditor::GetSceneParam(const std::string& param) const
    {
        auto settings = ECS.GetResource<SceneSettings>();
        auto params =
                ScriptRegistry::Get().ResolveParams(settings->SceneScript, settings->SceneParams);
        auto it = params.find(param);
        return it == params.end() ? 0.0f : it->second;
    }

    void SceneEditor::SetFieldSize(float width, float height)
    {
        RecordUndo();
        auto settings = ECS.GetResource<SceneSettings>();
        settings->FieldWidth = std::clamp(width, 1.0f, MAX_SIZE);
        settings->FieldHeight = std::clamp(height, 1.0f, MAX_SIZE);
        for (Entity e : SceneObjects::FindByTag(FIELD_NAME))
        {
            Shape2D& shape = ECS.GetComponent<Shape2D>(e);
            shape.Width = settings->FieldWidth;
            shape.Height = settings->FieldHeight;
            SceneObjects::ShapeChanged(e);
        }
        // Keep every object on the (possibly smaller) field
        for (Entity e : Objects())
        {
            if (!IsField(e))
            {
                Vec3 p = SceneObjects::GetPosition(e);
                Vec3 clamped = ClampToField(p);
                if (clamped.X != p.X || clamped.Z != p.Z)
                    SceneObjects::SetPosition(e, clamped);
            }
        }
        m_Dirty = true;
    }

    void SceneEditor::SetGameCamera(const Vec3& target, float distance)
    {
        RecordUndo();
        auto settings = ECS.GetResource<SceneSettings>();
        settings->CameraTarget = target;
        settings->CameraDistance = distance;
        m_Dirty = true;
    }

    //-----------------------------------------------------------------------------
    // Validation / files
    //-----------------------------------------------------------------------------

    std::vector<std::string> SceneEditor::Validate() const
    {
        std::vector<std::string> issues;
        const ScriptRegistry& scripts = ScriptRegistry::Get();
        std::set<std::string> names;
        for (Entity e : Objects())
        {
            const SceneObject& object = ECS.GetComponent<SceneObject>(e);
            if (!names.insert(object.Name).second)
                issues.push_back("Duplicate object name '" + object.Name + "'");
            if (!IsField(e))
            {
                Vec3 p = SceneObjects::GetPosition(e);
                Vec3 clamped = ClampToField(p);
                if (clamped.X != p.X || clamped.Z != p.Z)
                    issues.push_back("'" + object.Name + "' is outside the field");
            }
            std::string script = GetScript(e);
            if (!script.empty())
            {
                const ScriptInfo* info = scripts.Find(script);
                if (info == nullptr || info->IsSceneScript)
                    issues.push_back("'" + object.Name + "' uses unknown script '" + script + "'");
            }
        }
        const std::string& sceneScript = ECS.GetResource<SceneSettings>()->SceneScript;
        if (!sceneScript.empty())
        {
            const ScriptInfo* info = scripts.Find(sceneScript);
            if (info == nullptr || !info->IsSceneScript)
                issues.push_back("Unknown scene script '" + sceneScript + "'");
        }
        return issues;
    }

    std::vector<std::uint8_t> SceneEditor::SaveSceneToBytes(const std::string& name) const
    {
        ECS.GetResource<SceneSettings>()->Name = name;
        return Serializer().Save(ECS,
                                 {{GameManager::SCENE_KEY, PLAYER_SCENE},
                                  {META_NAME, name},
                                  {META_TOOL, "SceneEditor 2"}});
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
        auto scene = result.Metadata.find(GameManager::SCENE_KEY);
        if (scene == result.Metadata.end() || scene->second != PLAYER_SCENE)
        {
            result.Success = false;
            result.Error = "File is not an authored scene";
            return result;
        }

        std::vector<std::string> warnings = Serializer().Apply(ECS, snapshot);
        result.Warnings.insert(result.Warnings.end(), warnings.begin(), warnings.end());
        for (const std::string& issue : Validate())
            result.Warnings.push_back(issue);

        m_UndoStack.clear();
        m_RedoStack.clear();
        m_Selected = NULL_ENTITY;
        m_Dirty = false;
        m_Playing = false;
        WorldReplaced();
        return result;
    }

    //-----------------------------------------------------------------------------
    // Undo / redo / play
    //-----------------------------------------------------------------------------

    std::vector<std::uint8_t> SceneEditor::Snapshot() const
    {
        return Serializer().Save(ECS);
    }

    void SceneEditor::Restore(const std::vector<std::uint8_t>& snapshot)
    {
        Serialization::LoadResult result = Serializer().Load(ECS, snapshot);
        if (!result)
            throw Serialization::SerializationError("Snapshot failed to load: " + result.Error);
        if (!IsObject(m_Selected))
            m_Selected = NULL_ENTITY;
        WorldReplaced();
    }

    void SceneEditor::WorldReplaced()
    {
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
        if (m_UndoStack.empty() || m_Playing)
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
        if (m_RedoStack.empty() || m_Playing)
            return false;
        m_UndoStack.push_back(Snapshot());
        std::vector<std::uint8_t> snapshot = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();
        Restore(snapshot);
        m_Dirty = true;
        return true;
    }

    void SceneEditor::BeginPlay()
    {
        if (m_Playing)
            return;
        m_PlaySnapshot = Snapshot();
        m_Playing = true;
    }

    void SceneEditor::EndPlay()
    {
        if (!m_Playing)
            return;
        m_Playing = false;
        Restore(m_PlaySnapshot);
        m_PlaySnapshot.clear();
    }

    void SceneEditor::Select(Entity entity)
    {
        m_Selected = IsObject(entity) ? entity : NULL_ENTITY;
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

    Vec3 SceneEditor::ClampToField(const Vec3& position) const
    {
        auto settings = ECS.GetResource<SceneSettings>();
        float halfW = settings->FieldWidth * 0.5f;
        float halfH = settings->FieldHeight * 0.5f;
        Vec3 clamped = position;
        clamped.X = std::clamp(clamped.X, -halfW, halfW);
        clamped.Z = std::clamp(clamped.Z, -halfH, halfH);
        return clamped;
    }
} // namespace Editor
