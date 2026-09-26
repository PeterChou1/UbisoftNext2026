//---------------------------------------------------------------------------------
// ComponentManager.h
//---------------------------------------------------------------------------------
//
// Maps every component type (ComponentTypeID) to the ComponentBuffer holding
// the components of that type. Types are registered on first use
//

#pragma once

#include "ComponentBuffer.h"
#include "Entity.h"
#include "TypeID.h"

#include <cassert>
#include <memory>
#include <unordered_map>
#include <utility>

class ComponentManager
{
  public:
    template <typename T>
    ComponentTypeID GetComponentType()
    {
        ComponentTypeID typeID = TypeID<T>::VALUE;
        if (!IsRegistered(typeID))
        {
            assert(typeID < MAX_COMPONENTS && "Maximum allowed registered components exceeded");
            m_ComponentArrays.insert({typeID, std::make_unique<ComponentBuffer<T>>()});
        }
        return typeID;
    }

    template <typename... Ts>
    Signature GetSignature()
    {
        Signature signature;
        (signature.set(GetComponentType<Ts>()), ...);
        return signature;
    }

    template <typename T>
    void AddComponent(Entity entity, T component)
    {
        GetComponentType<T>();
        GetComponentArray<T>().InsertData(entity, std::move(component));
    }

    template <typename T>
    void RemoveComponent(Entity entity)
    {
        GetComponentArray<T>().RemoveData(entity);
    }

    template <typename T>
    bool HasComponent(Entity entity)
    {
        return IsRegistered(TypeID<T>::VALUE) && GetComponentArray<T>().HasData(entity);
    }

    template <typename T>
    T& GetComponent(Entity entity)
    {
        return GetComponentArray<T>().GetData(entity);
    }

    void EntityDestroyed(Entity entity)
    {
        for (auto& [typeID, buffer] : m_ComponentArrays)
            buffer->EntityDestroyed(entity);
    }

    void Clear() { m_ComponentArrays.clear(); }

  private:
    bool IsRegistered(ComponentTypeID typeID) const { return m_ComponentArrays.count(typeID) > 0; }

    template <typename T>
    ComponentBuffer<T>& GetComponentArray()
    {
        ComponentTypeID typeID = TypeID<T>::VALUE;
        assert(IsRegistered(typeID) && "Component does not exist");
        return static_cast<ComponentBuffer<T>&>(*m_ComponentArrays[typeID]);
    }

    std::unordered_map<ComponentTypeID, std::unique_ptr<IComponentBuffer>> m_ComponentArrays;
};
