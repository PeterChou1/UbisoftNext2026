//---------------------------------------------------------------------------------
// Prefab.h
//---------------------------------------------------------------------------------
//
// Prefabs: a group of objects (one root and its children) saved to a file and
// placed any number of times, like Unity prefabs.
//
//   Prefab::Data tower = Prefab::Capture(towerRoot, "tower");  // from the scene
//   Prefab::SaveFile(Prefab::PathOf("tower"), tower, error);    // data/prefabs/tower.ubprefab
//   Entity copy = Prefab::Instantiate(tower, {4, 0, 2}, 90.0f); // anywhere, any number of times
//
// A prefab stores every object of the group with:
//   - its name and tag;
//   - its parent in the group, and its local position / rotation / scale;
//   - what it is (empty, shape, model) and its physics body;
//   - its script and parameters;
//   - its reflected components (ComponentCatalog), saved by field name.
// Entity fields pointing inside the group are kept (they point at the new
// copies); references to objects outside the group are cleared.
//
// The root of every instance gets a PrefabLink naming its prefab. The editor
// uses it to update instances when the prefab is edited, to reset them, or to
// unpack them. Scripts spawn prefabs by name (Script::SpawnPrefab).
//
// Files are binary (magic UBPF, version, CRC) or plain text (UBPF-TEXT, one
// line per object) following WorldSerializer::FileFormat(). Loading checks
// everything (parents, enums, component data) before anything is created.
//
#pragma once

#include "../Assets.h"
#include "../Entity.h"
#include "../Quat.h"
#include "../Serialization/WorldSerializer.h"
#include "../Vec3.h"
#include "SceneComponents.h"
#include "SceneObjects.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Prefab
{
    constexpr const char* DIRECTORY = "data/prefabs";
    constexpr const char* EXTENSION = ".ubprefab";
    // 2: objects store their fragment / vertex shaders
    constexpr std::uint32_t FORMAT_VERSION = 2;

    enum class ObjectType
    {
        Empty,
        Shape,
        Model
    };

    /**
     * \brief A reflected component of an object, by catalog name
     */
    struct Component
    {
        std::string Name;
        std::vector<std::uint8_t> Data;
    };

    /**
     * \brief One object of a prefab. Objects are stored parents first: the
     *        first one is the root (Parent -1)
     */
    struct Object
    {
        std::string Name;
        std::string Tag;
        std::int32_t Parent = -1;
        // Relative to the parent. The root's position only keeps its height:
        // instances are placed where they are spawned
        Vec3 Position = {0.0f, 0.0f, 0.0f};
        Quat Rotation = Quat(0.0f, 0.0f, 0.0f, 1.0f);
        Vec3 Scale = {1.0f, 1.0f, 1.0f};
        ObjectType Type = ObjectType::Empty;
        Shape2D Shape;
        std::string Model;
        SceneObjects::BodyType Body = SceneObjects::BodyType::None;
        std::string Script;
        std::map<std::string, float> ScriptParams;
        std::vector<Component> Components;
        // Shapes and models only (version 1 files: the type's default)
        FragShaderTypeID FragShader = ShapeShaderID;
        VertShaderTypeID VertShader = DefaultVertShaderID;
    };

    struct Data
    {
        std::string Name;
        std::vector<Object> Objects;
    };

    /**
     * \brief Record `root` and all of its children (scene objects) as a prefab
     */
    Data Capture(Entity root, const std::string& name);

    /**
     * \brief Create a copy of the prefab: its root at `position` (the root's
     *        saved height is added) turned by `yawDegrees`, under `parent` when
     *        given. Names are made unique in the scene. Components the program
     *        does not know are skipped (with a warning). Returns the root
     */
    Entity Instantiate(const Data& prefab,
                       const Vec3& position,
                       float yawDegrees = 0.0f,
                       Entity parent = NULL_ENTITY,
                       std::vector<std::string>* warnings = nullptr);

    /**
     * \brief Objects of an instance, the root first (its whole subtree)
     */
    std::vector<Entity> InstanceObjects(Entity root);

    // -- Files -------------------------------------------------------------------

    std::vector<std::uint8_t> Save(const Data& prefab, Serialization::SaveFormat format);

    /**
     * \brief Parse and check a prefab file. False with `error` set when it is
     *        damaged; `warnings` lists unknown scripts / components
     */
    bool Load(const std::vector<std::uint8_t>& bytes,
              Data& prefab,
              std::string& error,
              std::vector<std::string>* warnings = nullptr);

    /**
     * \brief Save in WorldSerializer::FileFormat() (binary or text)
     */
    bool SaveFile(const std::string& path, const Data& prefab, std::string& error);
    bool LoadFile(const std::string& path,
                  Data& prefab,
                  std::string& error,
                  std::vector<std::string>* warnings = nullptr);

    /**
     * \brief data/prefabs/<name>.ubprefab (or in `directory`)
     */
    std::string PathOf(const std::string& name, const std::string& directory = DIRECTORY);

    /**
     * \brief Prefab names in a folder, sorted
     */
    std::vector<std::string> Available(const std::string& directory = DIRECTORY);

    /**
     * \brief A prefab loaded once from DIRECTORY and kept (nullptr when it can
     *        not be loaded). ClearCache after a prefab file changed
     */
    const Data* Find(const std::string& name);
    void ClearCache();

    /**
     * \brief File names only use letters, digits, _ and - (others become _)
     */
    std::string SafeName(const std::string& name);
} // namespace Prefab
