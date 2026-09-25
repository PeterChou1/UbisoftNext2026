//---------------------------------------------------------------------------------
// ComponentCatalog.h
//---------------------------------------------------------------------------------
//
// The project's own components, the ones the scene editor can add to and
// remove from objects. One call per component, at startup:
//
//     ComponentCatalog::Get().Register<Health>("Health", "Hit points");
//
// which:
//   - lists the component in the editor's "Add component" picker, with an
//     inspector generated from its REFLECT block (Reflection.h);
//   - saves it in scene files under the name "Health", field by field
//     (SceneSerializationRegistry);
//   - the ECS registers the type itself the first time it is added.
//
// The name is written into scene files: never rename it once scenes use it.
// Changing the fields is fine, they are matched by name when loading.
//
// Like scripts, registration is explicit (RegisterGameComponents in the game
// project) because the components live in a static library.
//
#pragma once

#include "../ECSManager.h"
#include "../Serialization/SceneSerialization.h"
#include "Reflection.h"

#include <functional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <vector>

struct ComponentEntry
{
    std::string Name;
    std::string Description;
    const Reflection::TypeInfo* Type = nullptr;
    std::type_index CppType = std::type_index(typeid(void));

    std::function<bool(ECSManager&, Entity)> Has;
    // Adds the component with its default values
    std::function<void(ECSManager&, Entity)> Add;
    std::function<void(ECSManager&, Entity)> Remove;
    // Copies the component from one entity to another (duplicates)
    std::function<void(ECSManager&, Entity, Entity)> Copy;
    // The component's memory, to read / write fields through Type
    std::function<void*(ECSManager&, Entity)> Data;
};

class ComponentCatalog
{
  public:
    static ComponentCatalog& Get();

    /**
     * \brief Make reflected component T available to the editor and to scene
     *        files. Registering the same type under the same name again does
     *        nothing
     * \param version layout version stored in files (ar.Version()); fields
     *        are matched by name, so it rarely needs to change
     */
    template <typename T>
    void Register(const std::string& name, const std::string& description = "", std::uint32_t version = 1)
    {
        static_assert(Reflection::IsReflectedV<T>, "Describe the component's fields with REFLECT(Type) first");
        std::type_index type(typeid(T));
        if (const ComponentEntry* existing = Find(name))
        {
            if (existing->CppType != type)
                throw std::logic_error("Component name registered twice: " + name);
            return;
        }
        for (const ComponentEntry& entry : m_Entries)
        {
            if (entry.CppType == type)
                throw std::logic_error("Component registered twice: " + entry.Name + " and " + name);
        }

        ComponentEntry entry;
        entry.Name = name;
        entry.Description = description;
        entry.Type = &Reflection::TypeInfoOf<T>();
        entry.CppType = type;
        entry.Has = [](ECSManager& ecs, Entity e) { return ecs.HasComponent<T>(e); };
        entry.Add = [](ECSManager& ecs, Entity e) { ecs.AddComponent<T>(e, T{}); };
        entry.Remove = [](ECSManager& ecs, Entity e) { ecs.RemoveComponent<T>(e); };
        entry.Copy = [](ECSManager& ecs, Entity from, Entity to) {
            T copy = ecs.GetComponent<T>(from);
            if (ecs.HasComponent<T>(to))
                ecs.GetComponent<T>(to) = copy;
            else
                ecs.AddComponent<T>(to, copy);
        };
        entry.Data = [](ECSManager& ecs, Entity e) -> void* { return &ecs.GetComponent<T>(e); };

        Serialization::SerializationRegistry& registry = Serialization::SceneSerializationRegistry();
        if (registry.FindComponent(name) == nullptr)
            registry.RegisterComponent<T>(name, version);
        m_Entries.push_back(std::move(entry));
    }

    const ComponentEntry* Find(const std::string& name) const
    {
        for (const ComponentEntry& entry : m_Entries)
        {
            if (entry.Name == name)
                return &entry;
        }
        return nullptr;
    }

    /**
     * \brief Every registered component, in registration order
     */
    const std::vector<ComponentEntry>& Entries() const { return m_Entries; }

    std::vector<std::string> Names() const
    {
        std::vector<std::string> names;
        for (const ComponentEntry& entry : m_Entries)
            names.push_back(entry.Name);
        return names;
    }

  private:
    std::vector<ComponentEntry> m_Entries;
};
