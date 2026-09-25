#include "SceneEditor.h"

#include "ECSManager.h"
#include "GameManager.h"
#include "Mesh.h"
#include "Reflection/ComponentCatalog.h"
#include "Scripting/ScriptRegistry.h"
#include "Serialization/SceneSerialization.h"
#include "FragShaderTag.h"
#include "Transform.h"
#include "VertShaderTag.h"
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

        // Empties are small targets: they win over the objects around them
        constexpr float EMPTY_AREA = 0.01f;

        float FootprintArea(Entity e)
        {
            if (SceneObjects::IsEmpty(e))
                return EMPTY_AREA;
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
        case ObjectKind::Empty:
            return "Empty";
        default:
            return "?";
        }
    }

    //-----------------------------------------------------------------------------
    // Scene lifetime
    //-----------------------------------------------------------------------------

    void SceneEditor::NewScene(bool withCamera)
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
        // The game camera, where the old fixed camera was
        if (withCamera)
            SceneCamera::Create(SceneCamera::FromSettings(*settings));

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
        Entity e = PlaceObject(kind, position, settings);
        m_Dirty = true;
        m_Selected = e;
        return e;
    }

    Entity SceneEditor::Create(ObjectKind kind, const Vec3& position, Entity parent, const PlaceSettings& settings)
    {
        if (kind >= ObjectKind::Count || (kind == ObjectKind::Model && settings.Model.empty()))
            return NULL_ENTITY;
        if (parent != NULL_ENTITY && !CanEdit(parent))
            return NULL_ENTITY;
        RecordUndo();
        Entity e = PlaceObject(kind, position, settings);
        if (parent != NULL_ENTITY)
            SceneObjects::SetParent(e, parent);
        m_Dirty = true;
        m_Selected = e;
        return e;
    }

    Entity SceneEditor::PlaceObject(ObjectKind kind, const Vec3& position, const PlaceSettings& settings)
    {
        Vec3 p = ClampToField(position);
        p.Y = 0.0f;
        Entity e = NULL_ENTITY;
        if (kind == ObjectKind::Empty)
        {
            // Just a transform: no body, it has nothing to collide with
            e = SceneObjects::CreateEmpty(SceneObjects::UniqueName(ObjectKindName(kind)), p, settings.YawDegrees);
            ECS.GetComponent<SceneObject>(e).Tag = settings.Tag;
        }
        else if (kind == ObjectKind::Model)
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
        return e;
    }

    //-----------------------------------------------------------------------------
    // Prefabs
    //-----------------------------------------------------------------------------

    Entity SceneEditor::PlacePrefab(const Prefab::Data& prefab, const Vec3& position, Entity parent)
    {
        if (prefab.Objects.empty() || (parent != NULL_ENTITY && !CanEdit(parent)))
            return NULL_ENTITY;
        RecordUndo();
        Vec3 p = ClampToField(position);
        p.Y = 0.0f;
        Entity e = Prefab::Instantiate(prefab, p, 0.0f, parent);
        m_Dirty = true;
        m_Selected = e;
        return e;
    }

    std::string SceneEditor::PrefabOf(Entity entity) const
    {
        if (!IsObject(entity) || !ECS.HasComponent<PrefabLink>(entity))
            return {};
        return ECS.GetComponent<PrefabLink>(entity).Prefab;
    }

    Prefab::Data SceneEditor::CapturePrefab(Entity root, const std::string& name) const
    {
        return Prefab::Capture(root, name);
    }

    bool SceneEditor::LinkPrefab(Entity root, const std::string& name)
    {
        if (!CanEdit(root) || name.empty())
            return false;
        RecordUndo();
        if (ECS.HasComponent<PrefabLink>(root))
            ECS.GetComponent<PrefabLink>(root).Prefab = name;
        else
            ECS.AddComponent<PrefabLink>(root, {name});
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::UnpackPrefab(Entity root)
    {
        if (PrefabOf(root).empty())
            return false;
        RecordUndo();
        ECS.RemoveComponent<PrefabLink>(root);
        m_Dirty = true;
        return true;
    }

    namespace
    {
        // Replace one instance, keeping its place (no undo step)
        Entity Replace(Entity root, const Prefab::Data& prefab)
        {
            Vec3 position = SceneObjects::GetPosition(root);
            float yaw = SceneObjects::GetYaw(root);
            Entity parent = SceneObjects::GetParent(root);
            std::string name = ECS.GetComponent<SceneObject>(root).Name;
            // The prefab's own root rotation is part of the instance's yaw
            float prefabYaw = prefab.Objects.front().Rotation.GetPitch2D() * 57.2957795f;
            // No FlushECS: the renderer drops the old meshes at the end of the frame
            SceneObjects::Destroy(root);
            Entity copy = Prefab::Instantiate(prefab, Vec3(position.X, 0.0f, position.Z), yaw - prefabYaw, parent);
            // Keep the root where it was (height included) and its name
            SceneObjects::SetPosition(copy, position);
            if (SceneObjects::FindByName(name) == NULL_ENTITY)
                ECS.GetComponent<SceneObject>(copy).Name = name;
            return copy;
        }
    } // namespace

    Entity SceneEditor::ResetToPrefab(Entity root, const Prefab::Data& prefab)
    {
        if (PrefabOf(root).empty() || prefab.Objects.empty())
            return NULL_ENTITY;
        RecordUndo();
        std::vector<Entity> old = Prefab::InstanceObjects(root);
        for (Entity e : old)
            ClearReferencesTo(e);
        bool wasSelected = std::find(old.begin(), old.end(), m_Selected) != old.end();
        Entity copy = Replace(root, prefab);
        if (wasSelected)
            m_Selected = copy;
        m_Dirty = true;
        return copy;
    }

    int SceneEditor::UpdatePrefabInstances(const Prefab::Data& prefab)
    {
        std::vector<Entity> instances;
        for (Entity e : Objects())
        {
            if (PrefabOf(e) == prefab.Name)
                instances.push_back(e);
        }
        if (instances.empty() || prefab.Objects.empty())
            return 0;
        RecordUndo();
        int updated = 0;
        for (Entity root : instances)
        {
            // An earlier update may already have replaced it (nested instances)
            if (PrefabOf(root) != prefab.Name)
                continue;
            for (Entity e : Prefab::InstanceObjects(root))
                ClearReferencesTo(e);
            Entity copy = Replace(root, prefab);
            if (m_Selected == root)
                m_Selected = copy;
            ++updated;
        }
        if (!IsObject(m_Selected))
            m_Selected = NULL_ENTITY;
        m_Dirty = true;
        return updated;
    }

    Prefab::Data SceneEditor::CaptureStage(const std::string& name) const
    {
        std::vector<Entity> roots;
        for (Entity e : RootObjects())
        {
            // The field and the game camera belong to the scene, not to a prefab
            if (!IsField(e) && !IsCamera(e))
                roots.push_back(e);
        }
        if (roots.size() == 1)
        {
            Prefab::Data single = Prefab::Capture(roots[0], name);
            return single;
        }
        // Several top level objects: grouped under a new empty root at the
        // stage's origin, each keeping its place relative to it
        Prefab::Data group;
        group.Name = name;
        Prefab::Object root;
        root.Name = name;
        root.Type = Prefab::ObjectType::Empty;
        group.Objects.push_back(root);
        for (Entity r : roots)
        {
            Prefab::Data part = Prefab::Capture(r, name);
            auto offset = static_cast<std::int32_t>(group.Objects.size());
            Transform world = ECS.GetComponent<Transform>(r).GetWorldTransform();
            for (std::size_t i = 0; i < part.Objects.size(); ++i)
            {
                Prefab::Object o = part.Objects[i];
                if (i == 0)
                {
                    o.Parent = 0;
                    o.Position = world.LocalPosition;
                }
                else
                    o.Parent += offset;
                group.Objects.push_back(std::move(o));
            }
        }
        return group;
    }

    Entity SceneEditor::OpenPrefabStage(const Prefab::Data* prefab, const std::string& name)
    {
        // A prefab is a group of objects, not a scene: no camera
        NewScene(false);
        Entity root = NULL_ENTITY;
        if (prefab != nullptr && !prefab->Objects.empty())
        {
            root = Prefab::Instantiate(*prefab, {0, 0, 0});
            // Inside its own stage the prefab is plain objects
            ECS.RemoveComponent<PrefabLink>(root);
        }
        else
            root = SceneObjects::CreateEmpty(name, {0, 0, 0});
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_Dirty = false;
        m_Selected = root;
        return root;
    }

    SceneEditor::Session SceneEditor::Suspend() const
    {
        Session session;
        session.World = Snapshot();
        session.Undo = m_UndoStack;
        session.Redo = m_RedoStack;
        session.Dirty = m_Dirty;
        session.Selected = m_Selected;
        return session;
    }

    void SceneEditor::Resume(const Session& session)
    {
        m_Playing = false;
        Restore(session.World);
        m_UndoStack = session.Undo;
        m_RedoStack = session.Redo;
        m_Dirty = session.Dirty;
        m_Selected = IsObject(session.Selected) ? session.Selected : NULL_ENTITY;
    }

    Entity SceneEditor::Duplicate(Entity source)
    {
        if (!CanEdit(source))
            return NULL_ENTITY;
        RecordUndo();
        Vec3 from = SceneObjects::GetPosition(source);
        Vec3 offset = ClampToField(from + DUPLICATE_OFFSET) - from;
        offset.Y = 0.0f;
        Entity copy = DuplicateTree(source, offset, ParentOf(source));
        m_Dirty = true;
        m_Selected = copy;
        return copy;
    }

    Entity SceneEditor::DuplicateTree(Entity source, const Vec3& offset, Entity parent)
    {
        Entity copy = CopyObject(source, SceneObjects::GetPosition(source) + offset);
        if (parent != NULL_ENTITY)
            SceneObjects::SetParent(copy, parent);
        for (Entity child : ChildrenOf(source))
            DuplicateTree(child, offset, copy);
        return copy;
    }

    Entity SceneEditor::CopyObject(Entity source, const Vec3& position)
    {
        const SceneObject& object = ECS.GetComponent<SceneObject>(source);
        Entity copy = NULL_ENTITY;
        if (SceneObjects::IsEmpty(source))
        {
            copy = SceneObjects::CreateEmpty(SceneObjects::UniqueName(object.Name), position, SceneObjects::GetYaw(source));
            ECS.GetComponent<SceneObject>(copy).Tag = object.Tag;
            SceneObjects::SetBodyType(copy, SceneObjects::GetBodyType(source));
        }
        else if (ECS.HasComponent<Shape2D>(source))
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
            // World scale: SetParent turns it back into the parent's scale
            copy = SceneObjects::CreateModel(SceneObjects::UniqueName(object.Name),
                                             ECS.GetComponent<Mesh>(source).Model,
                                             position,
                                             SceneObjects::GetYaw(source),
                                             ECS.GetComponent<Transform>(source).GetWorldTransform().LocalScale.X);
            ECS.GetComponent<SceneObject>(copy).Tag = object.Tag;
            SceneObjects::SetBodyType(copy, SceneObjects::GetBodyType(source));
        }
        if (ECS.HasComponent<ScriptComponent>(source))
            ECS.AddComponent<ScriptComponent>(copy, ECS.GetComponent<ScriptComponent>(source));
        if (HasShaders(source))
            SceneObjects::SetShaders(copy, SceneObjects::FragmentShaderOf(source), SceneObjects::VertexShaderOf(source));
        for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
        {
            if (entry.Has(ECS, source))
                entry.Copy(ECS, source, copy);
        }
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
        if (IsObject(entity) && SceneObjects::IsEmpty(entity))
            return ObjectKind::Empty;
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
        Entity camera = NULL_ENTITY;
        for (Entity e : Objects())
        {
            if (!SceneObjects::Contains(e, groundPoint, PICK_MARGIN))
                continue;
            if (IsField(e))
            {
                field = e;
                continue;
            }
            // A camera's target never hides the objects it looks at
            if (IsCamera(e))
            {
                camera = e;
                continue;
            }
            float area = FootprintArea(e);
            if (area < bestArea)
            {
                best = e;
                bestArea = area;
            }
        }
        if (best == NULL_ENTITY)
            best = camera;
        return best != NULL_ENTITY ? best : field;
    }

    Entity SceneEditor::PickRay(const std::function<Vec3(float)>& pointAtHeight, float* hitHeight) const
    {
        constexpr int SAMPLES = 8;
        Entity best = NULL_ENTITY;
        float bestArea = 1e30f;
        float bestHeight = 0.0f;
        Entity field = NULL_ENTITY;
        Entity camera = NULL_ENTITY;
        float cameraHeight = 0.0f;
        for (Entity e : Objects())
        {
            if (IsCamera(e))
            {
                // The eye marker (drawn in the air) picks the camera first;
                // its target only when nothing else is there
                Vec3 eye = SceneCamera::EyeOf(SceneCamera::ViewOf(e));
                Vec3 atEye = pointAtHeight(eye.Y);
                float dx = atEye.X - eye.X;
                float dz = atEye.Z - eye.Z;
                if (atEye.IsValid() && dx * dx + dz * dz <= CAMERA_PICK_RADIUS * CAMERA_PICK_RADIUS)
                {
                    best = e;
                    bestArea = -1.0f;
                    bestHeight = eye.Y;
                }
                else if (camera == NULL_ENTITY)
                {
                    float base = SceneObjects::GetPosition(e).Y;
                    Vec3 point = pointAtHeight(base);
                    if (point.IsValid() && SceneObjects::Contains(e, point, PICK_MARGIN))
                    {
                        camera = e;
                        cameraHeight = base;
                    }
                }
                continue;
            }
            // Where does the ray go through the object's body? Sampled from
            // the top down: the first hit is the surface the user sees
            float base = SceneObjects::GetPosition(e).Y;
            float height = ECS.HasComponent<Shape2D>(e) ? ECS.GetComponent<Shape2D>(e).Thickness
                           : SceneObjects::IsEmpty(e) ? 0.0f
                                                      : ECS.GetComponent<Transform>(e).GetWorldTransform().LocalScale.Y;
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
        if (best == NULL_ENTITY && camera != NULL_ENTITY)
        {
            best = camera;
            bestHeight = cameraHeight;
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

    bool SceneEditor::SetHeight(Entity entity, float y, bool recordUndo)
    {
        if (!CanEdit(entity) || !std::isfinite(y))
            return false;
        if (recordUndo)
            RecordUndo();
        Vec3 p = SceneObjects::GetPosition(entity);
        p.Y = std::clamp(y, -MAX_HEIGHT, MAX_HEIGHT);
        SceneObjects::SetPosition(entity, p);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::HasShaders(Entity entity) const
    {
        return IsObject(entity) && (ECS.HasComponent<Shape2D>(entity) || ECS.HasComponent<Mesh>(entity));
    }

    bool SceneEditor::SetFragmentShader(Entity entity, FragShaderTypeID shader)
    {
        if (!HasShaders(entity) || shader < DefaultFragShaderID || shader > StripesShaderID)
            return false;
        if (SceneObjects::FragmentShaderOf(entity) == shader && ECS.HasComponent<FragShaderTag>(entity))
            return true;
        RecordUndo();
        SceneObjects::SetFragmentShader(entity, shader);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetVertexShader(Entity entity, VertShaderTypeID shader)
    {
        if (!HasShaders(entity) || shader < DefaultVertShaderID || shader > SwayVertShaderID)
            return false;
        if (SceneObjects::VertexShaderOf(entity) == shader)
            return true;
        RecordUndo();
        SceneObjects::SetVertexShader(entity, shader);
        m_Dirty = true;
        return true;
    }

    Entity SceneEditor::GameCameraObject() const
    {
        Entity camera = SceneCamera::Find();
        return IsObject(camera) ? camera : NULL_ENTITY;
    }

    bool SceneEditor::IsCamera(Entity entity) const
    {
        return IsObject(entity) && ECS.HasComponent<GameCamera>(entity);
    }

    Entity SceneEditor::AddCamera(const Vec3& target)
    {
        RecordUndo();
        SceneCamera::View view;
        view.Target = ClampToField(target);
        view.Target.Y = 0.0f;
        bool first = GameCameraObject() == NULL_ENTITY;
        Entity e = SceneCamera::Create(view, SceneObjects::UniqueName(first ? SceneCamera::DEFAULT_NAME : "Camera"));
        m_Selected = e;
        m_Dirty = true;
        return e;
    }

    bool SceneEditor::SetCameraView(Entity camera, const SceneCamera::View& view)
    {
        if (!IsCamera(camera))
            return false;
        RecordUndo();
        SceneCamera::View placed = view;
        placed.Target = ClampToField(view.Target);
        SceneCamera::SetView(camera, placed);
        m_Dirty = true;
        return true;
    }

    Entity SceneEditor::SetGameCamera(const SceneCamera::View& view)
    {
        RecordUndo();
        SceneCamera::View placed = view;
        placed.Target = ClampToField(view.Target);
        Entity camera = GameCameraObject();
        if (camera == NULL_ENTITY)
            camera = SceneCamera::Create(placed, SceneObjects::UniqueName(SceneCamera::DEFAULT_NAME));
        else
            SceneCamera::SetView(camera, placed);
        // Kept in step for programs that still read the settings' camera
        auto settings = ECS.GetResource<SceneSettings>();
        settings->CameraTarget = placed.Target;
        settings->CameraDistance = ECS.GetComponent<GameCamera>(camera).Distance;
        m_Dirty = true;
        return camera;
    }

    bool SceneEditor::Remove(Entity entity)
    {
        if (!CanEdit(entity))
            return false;
        RecordUndo();
        // The object and its children go: nothing may point at them
        std::vector<Entity> removed = {entity};
        for (std::size_t i = 0; i < removed.size(); ++i)
        {
            for (Entity child : ChildrenOf(removed[i]))
                removed.push_back(child);
        }
        for (Entity e : removed)
            ClearReferencesTo(e);
        // No FlushECS: the render systems pick the deletion up next frame
        SceneObjects::Destroy(entity);
        if (std::find(removed.begin(), removed.end(), m_Selected) != removed.end())
            m_Selected = NULL_ENTITY;
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetParent(Entity child, Entity parent)
    {
        if (!CanEdit(child) || (parent != NULL_ENTITY && !CanEdit(parent)))
            return false;
        if (parent == child || SceneObjects::IsAncestor(child, parent))
            return false;
        if (ParentOf(child) == parent)
            return true;
        std::vector<std::uint8_t> snapshot = Snapshot();
        if (!SceneObjects::SetParent(child, parent))
            return false;
        PushUndo(std::move(snapshot));
        m_Dirty = true;
        return true;
    }

    Entity SceneEditor::AddEmpty(const Vec3& position, Entity parent)
    {
        if (parent != NULL_ENTITY && !CanEdit(parent))
            return NULL_ENTITY;
        RecordUndo();
        Vec3 p = ClampToField(position);
        p.Y = 0.0f;
        Entity e = SceneObjects::CreateEmpty(SceneObjects::UniqueName(ObjectKindName(ObjectKind::Empty)), p);
        if (parent != NULL_ENTITY)
            SceneObjects::SetParent(e, parent);
        m_Dirty = true;
        m_Selected = e;
        return e;
    }

    Entity SceneEditor::ParentOf(Entity entity) const
    {
        Entity parent = SceneObjects::GetParent(entity);
        return IsObject(parent) ? parent : NULL_ENTITY;
    }

    std::vector<Entity> SceneEditor::ChildrenOf(Entity entity) const
    {
        std::vector<Entity> result;
        for (Entity child : SceneObjects::GetChildren(entity))
        {
            if (IsObject(child))
                result.push_back(child);
        }
        return result;
    }

    std::vector<Entity> SceneEditor::RootObjects() const
    {
        std::vector<Entity> roots;
        for (Entity e : Objects())
        {
            if (ParentOf(e) != NULL_ENTITY)
                continue;
            if (IsField(e))
                roots.insert(roots.begin(), e);
            else
                roots.push_back(e);
        }
        return roots;
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
    // Components
    //-----------------------------------------------------------------------------

    std::vector<ComponentView> SceneEditor::ComponentsOf(Entity entity) const
    {
        std::vector<ComponentView> result;
        if (!IsObject(entity))
            return result;
        auto builtIn = [&](const char* name, bool removable, const std::string& summary) {
            ComponentView view;
            view.Name = name;
            view.BuiltIn = true;
            view.Removable = removable && CanEdit(entity);
            view.Summary = summary;
            result.push_back(view);
        };
        const SceneObject& object = ECS.GetComponent<SceneObject>(entity);
        std::size_t children = ChildrenOf(entity).size();
        Entity parent = ParentOf(entity);
        builtIn("Transform",
                false,
                parent != NULL_ENTITY ? "in " + NameOf(parent)
                : children > 0        ? std::to_string(children) + " children"
                                      : "");
        builtIn("SceneObject", false, object.Tag.empty() ? "" : "tag " + object.Tag);
        if (ECS.HasComponent<Shape2D>(entity))
            builtIn("Shape2D", false, ObjectKindName(KindOf(entity)));
        if (ECS.HasComponent<Mesh>(entity))
            builtIn("Mesh", false, ECS.GetComponent<Mesh>(entity).Model);
        BodyType body = SceneObjects::GetBodyType(entity);
        if (body != BodyType::None)
            builtIn(COMPONENT_RIGIDBODY, true, SceneObjects::BodyTypeName(body));
        if (ECS.HasComponent<ScriptComponent>(entity))
            builtIn(COMPONENT_SCRIPT, true, ECS.GetComponent<ScriptComponent>(entity).Script);

        for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
        {
            if (!entry.Has(ECS, entity))
                continue;
            ComponentView view;
            view.Name = entry.Name;
            view.Removable = CanEdit(entity);
            view.Type = entry.Type;
            result.push_back(view);
        }
        return result;
    }

    std::vector<std::string> SceneEditor::AddableComponents(Entity entity) const
    {
        std::vector<std::string> result;
        if (!CanEdit(entity))
            return result;
        if (SceneObjects::GetBodyType(entity) == BodyType::None)
            result.push_back(COMPONENT_RIGIDBODY);
        if (!ECS.HasComponent<ScriptComponent>(entity) && !ScriptRegistry::Get().Names(false).empty())
            result.push_back(COMPONENT_SCRIPT);
        for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
        {
            if (!entry.Has(ECS, entity))
                result.push_back(entry.Name);
        }
        return result;
    }

    bool SceneEditor::HasComponent(Entity entity, const std::string& component) const
    {
        for (const ComponentView& view : ComponentsOf(entity))
        {
            if (view.Name == component)
                return true;
        }
        return false;
    }

    bool SceneEditor::AddComponent(Entity entity, const std::string& component)
    {
        std::vector<std::string> addable = AddableComponents(entity);
        if (std::find(addable.begin(), addable.end(), component) == addable.end())
            return false;
        if (component == COMPONENT_RIGIDBODY)
            return SetBody(entity, BodyType::Static);
        if (component == COMPONENT_SCRIPT)
            return SetScript(entity, ScriptRegistry::Get().Names(false).front());
        const ComponentEntry* entry = ComponentCatalog::Get().Find(component);
        if (entry == nullptr)
            return false;
        RecordUndo();
        entry->Add(ECS, entity);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::RemoveComponent(Entity entity, const std::string& component)
    {
        if (!CanEdit(entity) || !HasComponent(entity, component))
            return false;
        if (component == COMPONENT_RIGIDBODY)
            return SetBody(entity, BodyType::None);
        if (component == COMPONENT_SCRIPT)
            return SetScript(entity, "");
        const ComponentEntry* entry = ComponentCatalog::Get().Find(component);
        if (entry == nullptr)
            return false;
        RecordUndo();
        entry->Remove(ECS, entity);
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::SetField(Entity entity,
                               const std::string& component,
                               const std::string& field,
                               const Reflection::FieldValue& value)
    {
        const ComponentEntry* entry = ComponentCatalog::Get().Find(component);
        if (!CanEdit(entity) || entry == nullptr || !entry->Has(ECS, entity))
            return false;
        const Reflection::FieldInfo* info = entry->Type->Find(field);
        if (info == nullptr || info->ReadOnly)
            return false;
        if (info->Type == Reflection::FieldType::Entity)
        {
            const auto* target = std::get_if<std::int64_t>(&value);
            if (target == nullptr ||
                (*target != static_cast<std::int64_t>(NULL_ENTITY) && !IsObject(static_cast<Entity>(*target))))
                return false;
        }
        void* data = entry->Data(ECS, entity);
        Reflection::FieldValue before = info->GetValue(data);
        std::vector<std::uint8_t> snapshot = Snapshot();
        if (!info->SetValue(data, value))
            return false;
        // Values that end up unchanged (clamped to the same value) are not an
        // edit: no undo step, the scene stays clean
        if (info->GetValue(data) == before)
            return true;
        PushUndo(std::move(snapshot));
        m_Dirty = true;
        return true;
    }

    bool SceneEditor::GetField(Entity entity,
                               const std::string& component,
                               const std::string& field,
                               Reflection::FieldValue& value) const
    {
        const ComponentEntry* entry = ComponentCatalog::Get().Find(component);
        if (!IsObject(entity) || entry == nullptr || !entry->Has(ECS, entity))
            return false;
        const Reflection::FieldInfo* info = entry->Type->Find(field);
        if (info == nullptr)
            return false;
        value = info->GetValue(entry->Data(ECS, entity));
        return true;
    }

    void SceneEditor::ClearReferencesTo(Entity removed)
    {
        const Reflection::FieldValue target = static_cast<std::int64_t>(removed);
        const Reflection::FieldValue none = static_cast<std::int64_t>(NULL_ENTITY);
        for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
        {
            for (const Reflection::FieldInfo& field : entry.Type->Fields)
            {
                if (field.Type != Reflection::FieldType::Entity)
                    continue;
                for (Entity e : ECS.GetLivingEntities())
                {
                    if (!entry.Has(ECS, e))
                        continue;
                    void* data = entry.Data(ECS, e);
                    if (field.GetValue(data) == target)
                        field.SetValue(data, none);
                }
            }
        }
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
        // Binary or plain text, following the program wide file format option
        return Serializer().Save(ECS,
                                 {{GameManager::SCENE_KEY, PLAYER_SCENE},
                                  {META_NAME, name},
                                  {META_TOOL, "SceneEditor 2"}},
                                 Serialization::WorldSerializer::FileFormat());
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
        PushUndo(Snapshot());
    }

    void SceneEditor::PushUndo(std::vector<std::uint8_t> snapshot)
    {
        m_UndoStack.push_back(std::move(snapshot));
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
