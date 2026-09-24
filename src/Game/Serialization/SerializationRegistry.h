//---------------------------------------------------------------------------------
// SerializationRegistry.h
//---------------------------------------------------------------------------------
//
// The registry maps every serializable Component and Resource to a stable
// string name and a version number.
//
// Component TypeIDs in the ECS are generated at static initialization time so
// their numeric values can change between builds. Save files therefore
// identify types by the name given at registration and never by TypeID.
//
// Registering a type:
//
//     registry.RegisterComponent<Transform>("Transform");
//     registry.RegisterComponent<Health>("Health", 2);   // version 2 layout
//     registry.RegisterResource<GameState>("GameState");
//
// A type is serialized through its Serialize(Archive&, T&) function (see
// Archive.h). While loading, ar.Version() returns the version stored in the
// file, which lets a Serialize function read older layouts:
//
//     template <typename Archive>
//     void Serialize(Archive& ar, Health& h)
//     {
//         ar(h.Current);
//         if (ar.Version() >= 2)
//             ar(h.Max);
//     }
//
// Components which are not registered are simply not saved. This is the way
// to exclude purely runtime data (render caches, behaviour tree tags ...)
//
#pragma once

#include "../ECSManager.h"
#include "Archive.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Serialization
{
    /**
     * \brief Deferred change to the ECS produced while parsing a save file.
     *        Parsing never touches the ECS, the changes are only applied once
     *        the whole file has been validated
     */
    using StagedAction = std::function<void(ECSManager&)>;

    /**
     * \brief Type erased serializer for a single Component type
     */
    struct ComponentSerializer
    {
        std::string Name;
        std::uint32_t Version = 1;
        std::function<bool(ECSManager&, Entity)> Has;
        std::function<void(ECSManager&, Entity, OutputArchive&)> Save;
        // Parses a component from the archive and returns the action which adds
        // it to the given entity
        std::function<StagedAction(Entity, InputArchive&)> Load;
    };

    /**
     * \brief Type erased serializer for a single Resource type
     */
    struct ResourceSerializer
    {
        std::string Name;
        std::uint32_t Version = 1;
        std::function<bool(ECSManager&)> Has;
        std::function<void(ECSManager&, OutputArchive&)> Save;
        // Parses into a temporary instance to validate the data (no side effects)
        std::function<void(InputArchive&)> Validate;
        // Deserializes directly into the live resource, only fields touched by
        // the Serialize function are modified
        std::function<void(ECSManager&, InputArchive&)> Apply;
    };

    class SerializationRegistry
    {
      public:
        /**
         * \brief Register Component T under a stable name
         * \param name identifier written to the save file, never change it once
         *        save files exist (bump the version instead)
         * \param version current layout version of T (starting at 1)
         */
        template <typename T>
        void RegisterComponent(const std::string& name, std::uint32_t version = 1)
        {
            CheckRegistration(name, version, std::type_index(typeid(T)), m_ComponentByName);
            ComponentSerializer s;
            s.Name = name;
            s.Version = version;
            s.Has = [](ECSManager& ecs, Entity e) { return ecs.HasComponent<T>(e); };
            s.Save = [](ECSManager& ecs, Entity e, OutputArchive& ar) {
                ar(ecs.GetComponent<T>(e));
            };
            s.Load = [](Entity e, InputArchive& ar) -> StagedAction {
                T component{};
                ar(component);
                return [e, component](ECSManager& ecs) { ecs.AddComponent<T>(e, component); };
            };
            m_ComponentByName[name] = m_Components.size();
            m_Components.push_back(std::move(s));
        }

        /**
         * \brief Register Resource T under a stable name
         */
        template <typename T>
        void RegisterResource(const std::string& name, std::uint32_t version = 1)
        {
            CheckRegistration(name, version, std::type_index(typeid(T)), m_ResourceByName);
            ResourceSerializer s;
            s.Name = name;
            s.Version = version;
            s.Has = [](ECSManager& ecs) { return ecs.HasResource<T>(); };
            s.Save = [](ECSManager& ecs, OutputArchive& ar) { ar(*ecs.GetResource<T>()); };
            s.Validate = [](InputArchive& ar) {
                auto scratch = std::make_unique<T>();
                ar(*scratch);
            };
            s.Apply = [](ECSManager& ecs, InputArchive& ar) { ar(*ecs.GetResource<T>()); };
            m_ResourceByName[name] = m_Resources.size();
            m_Resources.push_back(std::move(s));
        }

        /**
         * \brief Callback executed after a world was restored, used to rebuild
         *        derived runtime state which is not stored in the save file
         */
        void AddPostLoadCallback(std::function<void(ECSManager&)> callback)
        {
            m_PostLoadCallbacks.push_back(std::move(callback));
        }

        const std::vector<ComponentSerializer>& Components() const { return m_Components; }

        const std::vector<ResourceSerializer>& Resources() const { return m_Resources; }

        const std::vector<std::function<void(ECSManager&)>>& PostLoadCallbacks() const
        {
            return m_PostLoadCallbacks;
        }

        const ComponentSerializer* FindComponent(const std::string& name) const
        {
            auto it = m_ComponentByName.find(name);
            return it == m_ComponentByName.end() ? nullptr : &m_Components[it->second];
        }

        const ResourceSerializer* FindResource(const std::string& name) const
        {
            auto it = m_ResourceByName.find(name);
            return it == m_ResourceByName.end() ? nullptr : &m_Resources[it->second];
        }

      private:
        void CheckRegistration(const std::string& name,
                               std::uint32_t version,
                               std::type_index type,
                               const std::unordered_map<std::string, size_t>& byName)
        {
            if (name.empty())
                throw SerializationError("Serializable type name can not be empty");
            if (version == 0)
                throw SerializationError("Serializable type version must start at 1");
            if (byName.count(name) != 0)
                throw SerializationError("Serializable type name registered twice: " + name);
            if (!m_RegisteredTypes.insert(type).second)
                throw SerializationError("Serializable type registered twice: " + name);
        }

        std::vector<ComponentSerializer> m_Components;
        std::vector<ResourceSerializer> m_Resources;
        std::unordered_map<std::string, size_t> m_ComponentByName;
        std::unordered_map<std::string, size_t> m_ResourceByName;
        std::set<std::type_index> m_RegisteredTypes;
        std::vector<std::function<void(ECSManager&)>> m_PostLoadCallbacks;
    };
} // namespace Serialization
