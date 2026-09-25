#include "Prefab.h"

#include "../ECSManager.h"
#include "../Log.h"
#include "../Mesh.h"
#include "../Reflection/ComponentCatalog.h"
#include "../Scripting/ScriptRegistry.h"
#include "../Serialization/Crc32.h"
#include "../Serialization/SceneSerialization.h"
#include "../Serialization/TextArchive.h"
#include "../Transform.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <unordered_map>

extern ECSManager ECS;

SERIALIZATION_ENUM_RANGE(Prefab::ObjectType, Prefab::ObjectType::Empty, Prefab::ObjectType::Model)
SERIALIZATION_ENUM_RANGE(SceneObjects::BodyType, SceneObjects::BodyType::None, SceneObjects::BodyType::Trigger)

namespace Prefab
{
    template <typename Archive>
    void Serialize(Archive& ar, Component& component)
    {
        ar(component.Name);
        if constexpr (Archive::IsSaving)
        {
            ar.WriteSize(component.Data.size());
            if (!component.Data.empty())
                ar.WriteBytes(component.Data.data(), component.Data.size());
        }
        else
        {
            std::size_t size = ar.ReadSize();
            component.Data.resize(size);
            if (size > 0)
                ar.ReadBytes(component.Data.data(), size);
        }
    }

    template <typename Archive>
    void Serialize(Archive& ar, Object& object)
    {
        ar(object.Name, object.Tag, object.Parent);
        ar(object.Position, object.Rotation, object.Scale);
        ar(object.Type, object.Shape, object.Model, object.Body);
        ar(object.Script, object.ScriptParams, object.Components);
        if (ar.Version() >= 2)
            ar(object.FragShader, object.VertShader);
        else if constexpr (Archive::IsLoading)
        {
            object.FragShader = object.Type == ObjectType::Model ? BlinnPhongID : ShapeShaderID;
            object.VertShader = DefaultVertShaderID;
        }
    }

    namespace
    {
        constexpr std::uint8_t MAGIC[4] = {'U', 'B', 'P', 'F'};
        constexpr const char* TEXT_MAGIC = "UBPF-TEXT";
        constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;

        std::unordered_map<std::string, Data>& Cache()
        {
            static std::unordered_map<std::string, Data> cache;
            return cache;
        }

        bool IsSceneObject(Entity e)
        {
            return e != NULL_ENTITY && ECS.IsEntityAlive(e) && ECS.HasComponent<SceneObject>(e) &&
                   ECS.HasComponent<Transform>(e);
        }

        bool Finite(const Vec3& v) { return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z); }

        /**
         * \brief Rewrite every Entity field of the reflected components of
         *        `e` through `map` (the component data is edited in place)
         */
        template <typename Map>
        void RemapEntityFields(Entity e, const Map& map)
        {
            for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
            {
                if (!entry.Has(ECS, e))
                    continue;
                for (const Reflection::FieldInfo& field : entry.Type->Fields)
                {
                    if (field.Type != Reflection::FieldType::Entity)
                        continue;
                    void* data = entry.Data(ECS, e);
                    auto value = std::get<std::int64_t>(field.GetValue(data));
                    field.SetValue(data, static_cast<std::int64_t>(map(static_cast<Entity>(value))));
                }
            }
        }

        /**
         * \brief Structure and values that make no sense are refused
         */
        void Check(const Data& prefab, std::vector<std::string>* warnings)
        {
            using Serialization::SerializationError;
            if (prefab.Objects.empty())
                throw SerializationError("the prefab has no objects");
            for (std::size_t i = 0; i < prefab.Objects.size(); ++i)
            {
                const Object& o = prefab.Objects[i];
                std::string where = "object " + std::to_string(i) + " '" + o.Name + "': ";
                bool rootOk = i == 0 ? o.Parent == -1 : o.Parent >= 0 && o.Parent < static_cast<std::int32_t>(i);
                if (!rootOk)
                    throw SerializationError(where + "its parent must be an earlier object (the first is the root)");
                if (!Finite(o.Position) || !Finite(o.Scale) || !std::isfinite(o.Rotation.X) ||
                    !std::isfinite(o.Rotation.Y) || !std::isfinite(o.Rotation.Z) || !std::isfinite(o.Rotation.W))
                    throw SerializationError(where + "position, rotation or scale is not a number");
                if (o.Type == ObjectType::Model && o.Model.empty())
                    throw SerializationError(where + "a model object needs a model name");
                if (o.Type == ObjectType::Shape &&
                    (!(o.Shape.Width >= 0.0f) || !(o.Shape.Height >= 0.0f) || !(o.Shape.Thickness >= 0.0f)))
                    throw SerializationError(where + "shape sizes must be positive numbers");
                if (!o.Script.empty() && ScriptRegistry::Get().Find(o.Script) == nullptr && warnings != nullptr)
                    warnings->push_back(where + "unknown script '" + o.Script + "'");
                for (const Component& c : o.Components)
                {
                    const ComponentEntry* entry = ComponentCatalog::Get().Find(c.Name);
                    if (entry == nullptr)
                    {
                        if (warnings != nullptr)
                            warnings->push_back(where + "unknown component '" + c.Name + "' is skipped");
                        continue;
                    }
                    try
                    {
                        entry->ValidateBytes(c.Data);
                    }
                    catch (const SerializationError& e)
                    {
                        throw SerializationError(where + "component " + c.Name + ": " + e.what());
                    }
                }
            }
        }

        void ParseBinary(const std::vector<std::uint8_t>& bytes, Data& prefab)
        {
            using Serialization::SerializationError;
            Serialization::InputArchive header(bytes);
            std::uint8_t magic[4] = {};
            header.ReadBytes(magic, 4);
            if (!std::equal(magic, magic + 4, MAGIC))
                throw SerializationError("not a prefab file");
            std::uint32_t version = 0, size = 0, crc = 0;
            header(version, size, crc);
            if (version == 0 || version > FORMAT_VERSION)
                throw SerializationError("prefab format version " + std::to_string(version) +
                                         " is newer than this program");
            if (size != header.Remaining())
                throw SerializationError("the file is truncated or has trailing data");
            const std::uint8_t* payload = bytes.data() + (bytes.size() - size);
            if (Serialization::Crc32(payload, size) != crc)
                throw SerializationError("checksum mismatch (the file is damaged)");
            Serialization::InputArchive ar(payload, size);
            ar.SetVersion(version);
            ar(prefab.Name, prefab.Objects);
            if (!ar.AtEnd())
                throw SerializationError("trailing data after the prefab");
        }

        void ParseText(const std::string& text, Data& prefab)
        {
            using Serialization::SerializationError;
            using Serialization::TextInputArchive;
            std::istringstream in(text);
            std::string line;
            std::size_t number = 0;
            auto next = [&]() {
                while (std::getline(in, line))
                {
                    ++number;
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (!line.empty() && line[0] != '#')
                        return true;
                }
                return false;
            };
            if (!next() || line.rfind(TEXT_MAGIC, 0) != 0)
                throw SerializationError("not a prefab file");
            std::uint32_t version = 0;
            {
                TextInputArchive header(line.substr(std::string(TEXT_MAGIC).size()), number);
                header(version);
                if (version == 0 || version > FORMAT_VERSION)
                    header.Fail("prefab format version " + std::to_string(version) + " is newer than this program");
            }
            if (!next() || line.rfind("prefab ", 0) != 0)
                throw SerializationError("line " + std::to_string(number) + ": expected 'prefab \"name\" [count]'");
            TextInputArchive record(line.substr(7), number);
            record(prefab.Name);
            // The objects follow on their own lines: the count is a plain number
            std::uint32_t count = 0;
            record(count);
            if (count > MAX_ENTITIES)
                record.Fail("too many objects: " + std::to_string(count));
            if (!record.AtEnd())
                record.Fail("unexpected '" + record.Rest() + "'");
            prefab.Objects.clear();
            for (std::uint32_t i = 0; i < count; ++i)
            {
                if (!next() || line.rfind("object ", 0) != 0)
                    throw SerializationError("line " + std::to_string(number) + ": expected an object record");
                TextInputArchive object(line.substr(7), number);
                object.SetVersion(version);
                Object o;
                object(o);
                if (!object.AtEnd())
                    object.Fail("unexpected '" + object.Rest() + "'");
                prefab.Objects.push_back(std::move(o));
            }
            if (!next() || line != "end")
                throw SerializationError("line " + std::to_string(number) + ": expected 'end'");
        }
    } // namespace

    //-----------------------------------------------------------------------------
    // Capture / instantiate
    //-----------------------------------------------------------------------------

    std::vector<Entity> InstanceObjects(Entity root)
    {
        std::vector<Entity> objects;
        if (!IsSceneObject(root))
            return objects;
        objects.push_back(root);
        // Breadth first: parents always come before their children
        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            for (Entity child : SceneObjects::GetChildren(objects[i]))
            {
                if (IsSceneObject(child))
                    objects.push_back(child);
            }
        }
        return objects;
    }

    Data Capture(Entity root, const std::string& name)
    {
        Data prefab;
        prefab.Name = name;
        std::vector<Entity> objects = InstanceObjects(root);
        std::unordered_map<Entity, std::int32_t> index;
        for (std::size_t i = 0; i < objects.size(); ++i)
            index[objects[i]] = static_cast<std::int32_t>(i);
        // Entity fields are stored as "object number + 1" inside the prefab
        // (0 = none or outside the group)
        auto toPrefab = [&](Entity e) -> Entity {
            auto it = index.find(e);
            return it == index.end() ? NULL_ENTITY : static_cast<Entity>(it->second + 1);
        };

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            Entity e = objects[i];
            const SceneObject& so = ECS.GetComponent<SceneObject>(e);
            Transform& t = ECS.GetComponent<Transform>(e);
            Object o;
            o.Name = so.Name;
            o.Tag = so.Tag;
            if (i == 0)
            {
                Transform world = t.GetWorldTransform();
                o.Position = Vec3(0.0f, world.LocalPosition.Y, 0.0f);
                o.Rotation = world.LocalRotation;
                o.Scale = world.LocalScale;
            }
            else
            {
                o.Parent = index.at(t.Parent);
                o.Position = t.LocalPosition;
                o.Rotation = t.LocalRotation;
                o.Scale = t.LocalScale;
            }
            if (ECS.HasComponent<Shape2D>(e))
            {
                o.Type = ObjectType::Shape;
                o.Shape = ECS.GetComponent<Shape2D>(e);
                o.Shape.Built = false;
            }
            else if (ECS.HasComponent<Mesh>(e))
            {
                o.Type = ObjectType::Model;
                o.Model = ECS.GetComponent<Mesh>(e).Model;
            }
            if (ECS.HasComponent<FragShaderTag>(e))
                o.FragShader = ECS.GetComponent<FragShaderTag>(e).FragAssetId;
            if (ECS.HasComponent<VertShaderTag>(e))
                o.VertShader = ECS.GetComponent<VertShaderTag>(e).VertAssetId;
            o.Body = SceneObjects::GetBodyType(e);
            if (ECS.HasComponent<ScriptComponent>(e))
            {
                o.Script = ECS.GetComponent<ScriptComponent>(e).Script;
                o.ScriptParams = ECS.GetComponent<ScriptComponent>(e).Params;
            }
            // Components with their Entity fields rewritten for the prefab,
            // then put back as they were
            std::vector<std::pair<const ComponentEntry*, std::vector<std::uint8_t>>> originals;
            for (const ComponentEntry& entry : ComponentCatalog::Get().Entries())
            {
                if (entry.Has(ECS, e))
                    originals.emplace_back(&entry, entry.SaveBytes(ECS, e));
            }
            RemapEntityFields(e, toPrefab);
            for (auto& [entry, bytes] : originals)
            {
                o.Components.push_back({entry->Name, entry->SaveBytes(ECS, e)});
                entry->LoadBytes(ECS, e, bytes);
            }
            prefab.Objects.push_back(std::move(o));
        }
        return prefab;
    }

    Entity Instantiate(const Data& prefab,
                       const Vec3& position,
                       float yawDegrees,
                       Entity parent,
                       std::vector<std::string>* warnings)
    {
        if (prefab.Objects.empty())
            return NULL_ENTITY;
        std::vector<Entity> created;
        for (std::size_t i = 0; i < prefab.Objects.size(); ++i)
        {
            const Object& o = prefab.Objects[i];
            std::string name = SceneObjects::UniqueName(o.Name.empty() ? std::string("Object") : o.Name);
            Entity e = NULL_ENTITY;
            if (o.Type == ObjectType::Shape)
            {
                SceneObjects::ShapeDesc desc;
                desc.Name = name;
                desc.Shape = o.Shape;
                e = SceneObjects::CreateShape(desc);
            }
            else if (o.Type == ObjectType::Model)
                e = SceneObjects::CreateModel(name, o.Model, {0, 0, 0});
            else
                e = SceneObjects::CreateEmpty(name, {0, 0, 0});
            ECS.GetComponent<SceneObject>(e).Tag = o.Tag;
            if (o.Type != ObjectType::Empty)
                SceneObjects::SetShaders(e, o.FragShader, o.VertShader);

            Transform& t = ECS.GetComponent<Transform>(e);
            if (i == 0)
            {
                // The root goes where it is spawned
                if (parent != NULL_ENTITY)
                    SceneObjects::SetParent(e, parent);
                Quat turn(Vec3(0, 1, 0), yawDegrees * DEG_TO_RAD);
                t.SetLocalPose({0, 0, 0}, turn * o.Rotation, o.Scale);
                SceneObjects::SetPosition(e, position + Vec3(0.0f, o.Position.Y, 0.0f));
            }
            else
            {
                Entity p = created[static_cast<std::size_t>(o.Parent)];
                t.Parent = p;
                ECS.GetComponent<Transform>(p).Children.push_back(e);
                t.SetLocalPose(o.Position, o.Rotation, o.Scale);
            }
            // Bodies are built from the final size and place
            SceneObjects::SetBodyType(e, o.Body);
            if (!o.Script.empty())
                ECS.AddComponent<ScriptComponent>(e, {o.Script, o.ScriptParams});
            for (const Component& c : o.Components)
            {
                const ComponentEntry* entry = ComponentCatalog::Get().Find(c.Name);
                if (entry == nullptr)
                {
                    if (warnings != nullptr)
                        warnings->push_back("unknown component '" + c.Name + "' skipped");
                    continue;
                }
                entry->LoadBytes(ECS, e, c.Data);
            }
            created.push_back(e);
        }
        // Entity fields: object number + 1 -> the new copy
        auto toScene = [&](Entity value) -> Entity {
            return value >= 1 && value <= created.size() ? created[value - 1] : NULL_ENTITY;
        };
        for (Entity e : created)
            RemapEntityFields(e, toScene);
        ECS.AddComponent<PrefabLink>(created[0], {prefab.Name});
        return created[0];
    }

    //-----------------------------------------------------------------------------
    // Files
    //-----------------------------------------------------------------------------

    std::vector<std::uint8_t> Save(const Data& prefab, Serialization::SaveFormat format)
    {
        Data copy = prefab;
        if (format == Serialization::SaveFormat::Text)
        {
            Serialization::TextOutputArchive ar;
            ar.SetVersion(FORMAT_VERSION);
            ar.Raw(std::string(TEXT_MAGIC) + " " + std::to_string(FORMAT_VERSION));
            ar.NewLine();
            ar.Raw("# Prefab: one line per object, the root first. Object fields: name, tag, parent\n"
                   "# (-1 = root), position, rotation, scale, type (0 empty 1 shape 2 model), shape,\n"
                   "# model, body (0 none 1 static 2 dynamic 3 trigger), script, parameters, components,\n"
                   "# fragment shader, vertex shader (see Assets.h)\n");
            ar.Raw("prefab");
            auto count = static_cast<std::uint32_t>(copy.Objects.size());
            ar(copy.Name, count);
            ar.NewLine();
            for (Object& o : copy.Objects)
            {
                ar.Raw("object");
                ar(o);
                ar.NewLine();
            }
            ar.Raw("end");
            ar.NewLine();
            return std::vector<std::uint8_t>(ar.Text().begin(), ar.Text().end());
        }
        Serialization::OutputArchive payload;
        payload.SetVersion(FORMAT_VERSION);
        payload(copy.Name, copy.Objects);
        Serialization::OutputArchive ar;
        ar.WriteBytes(MAGIC, sizeof(MAGIC));
        std::uint32_t version = FORMAT_VERSION;
        auto size = static_cast<std::uint32_t>(payload.Size());
        std::uint32_t crc = Serialization::Crc32(payload.Buffer().data(), payload.Size());
        ar(version, size, crc);
        ar.WriteBytes(payload.Buffer().data(), payload.Size());
        return ar.TakeBuffer();
    }

    bool Load(const std::vector<std::uint8_t>& bytes, Data& prefab, std::string& error, std::vector<std::string>* warnings)
    {
        Data parsed;
        std::vector<std::string> found;
        try
        {
            std::string head(bytes.begin(), bytes.begin() + std::min<std::size_t>(bytes.size(), 9));
            if (head == TEXT_MAGIC)
                ParseText(std::string(bytes.begin(), bytes.end()), parsed);
            else
                ParseBinary(bytes, parsed);
            Check(parsed, &found);
        }
        catch (const Serialization::SerializationError& e)
        {
            error = e.what();
            return false;
        }
        prefab = std::move(parsed);
        if (warnings != nullptr)
            warnings->insert(warnings->end(), found.begin(), found.end());
        return true;
    }

    bool SaveFile(const std::string& path, const Data& prefab, std::string& error)
    {
        bool ok = Serialization::WorldSerializer::WriteFile(
                path, Save(prefab, Serialization::WorldSerializer::FileFormat()), error);
        ClearCache();
        if (ok)
            LOG_INFO("Prefab", "Saved %s (%zu objects)", path.c_str(), prefab.Objects.size());
        return ok;
    }

    bool LoadFile(const std::string& path, Data& prefab, std::string& error, std::vector<std::string>* warnings)
    {
        std::vector<std::uint8_t> bytes;
        if (!Serialization::WorldSerializer::ReadFile(path, bytes, error))
            return false;
        if (!Load(bytes, prefab, error, warnings))
        {
            error = path + ": " + error;
            return false;
        }
        return true;
    }

    std::string PathOf(const std::string& name, const std::string& directory)
    {
        return (std::filesystem::path(directory) / (name + EXTENSION)).string();
    }

    std::vector<std::string> Available(const std::string& directory)
    {
        std::vector<std::string> names;
        std::error_code ec;
        for (const auto& file : std::filesystem::directory_iterator(directory, ec))
        {
            if (file.path().extension() == EXTENSION)
                names.push_back(file.path().stem().string());
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    const Data* Find(const std::string& name)
    {
        auto it = Cache().find(name);
        if (it != Cache().end())
            return &it->second;
        Data prefab;
        std::string error;
        if (!LoadFile(PathOf(name), prefab, error))
        {
            LOG_WARN("Prefab", "Can not load prefab %s: %s", name.c_str(), error.c_str());
            return nullptr;
        }
        return &Cache().emplace(name, std::move(prefab)).first->second;
    }

    void ClearCache() { Cache().clear(); }

    std::string SafeName(const std::string& name)
    {
        std::string safe;
        for (char c : name)
        {
            bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
            safe += ok ? c : '_';
        }
        return safe.empty() ? std::string("prefab") : safe;
    }
} // namespace Prefab
