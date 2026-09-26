//---------------------------------------------------------------------------------
// VisitorManager.h
//---------------------------------------------------------------------------------
//
// Manages all Visitors of the ECS System (see Visitor.h), one per set of
// component types, and keeps their entities up to date
//
#pragma once

#include "Entity.h"
#include "Visitor.h"

#include <cassert>
#include <typeinfo>
#include <unordered_map>

class VisitorManager
{
  public:
    /**
     * \brief The Visitor of the component types Ts, nullptr if not registered
     */
    template <typename... Ts>
    Visitor* Find()
    {
        auto visitor = m_Visitors.find(Key<Ts...>());
        return visitor == m_Visitors.end() ? nullptr : &visitor->second;
    }

    template <typename... Ts>
    Visitor& Register(Signature requirements)
    {
        assert(Find<Ts...>() == nullptr && "Registering system more than once.");
        Visitor& visitor = m_Visitors[Key<Ts...>()];
        visitor.Requirements = requirements;
        return visitor;
    }

    void EntityDestroyed(Entity entity)
    {
        for (auto& [key, visitor] : m_Visitors)
            visitor.Entities.erase(entity);
    }

    void EntitySignatureDeleted(Entity entity, Signature entitySignature)
    {
        for (auto& [key, visitor] : m_Visitors)
        {
            if ((entitySignature & visitor.Requirements) == visitor.Requirements)
                visitor.DeletedEntities.insert(entity);
        }
    }

    void EntitySignatureChanged(Entity entity, Signature entitySignature)
    {
        for (auto& [key, visitor] : m_Visitors)
        {
            if ((entitySignature & visitor.Requirements) == visitor.Requirements)
                visitor.Entities.insert(entity);
            else
                visitor.Entities.erase(entity);
        }
    }

    void FlushDeletedEntities()
    {
        for (auto& [key, visitor] : m_Visitors)
            visitor.DeletedEntities.clear();
    }

    void Clear() { m_Visitors.clear(); }

  private:
    template <typename... Ts>
    struct Components
    {
    };

    // Visitors are identified by the type name of Components<Ts...>
    template <typename... Ts>
    static const char* Key()
    {
        return typeid(Components<Ts...>).name();
    }

    // (node based: references to the visitors stay valid)
    std::unordered_map<const char*, Visitor> m_Visitors;
};
