//---------------------------------------------------------------------------------
// ComponentBuffer.h
//---------------------------------------------------------------------------------
//
// The components of one type, packed in a contiguous array for fast cache
// access
//

#pragma once

#include "Entity.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <unordered_map>
#include <utility>

class IComponentBuffer
{
  public:
    virtual ~IComponentBuffer() = default;
    virtual void EntityDestroyed(Entity entity) = 0;
};

template <typename T>
class ComponentBuffer : public IComponentBuffer
{
  public:
    void InsertData(Entity entity, T component)
    {
        assert(!HasData(entity) && "Component added to same entity more than once.");
        // Put new entry at end
        m_EntityToIndex[entity] = m_Size;
        m_IndexToEntity[m_Size] = entity;
        m_Components[m_Size] = std::move(component);
        ++m_Size;
    }

    void RemoveData(Entity entity)
    {
        assert(HasData(entity) && "Removing non-existent component.");

        // Copy the last element into the removed one's place to keep the
        // array dense
        std::size_t removedIndex = m_EntityToIndex[entity];
        std::size_t lastIndex = m_Size - 1;
        m_Components[removedIndex] = m_Components[lastIndex];

        Entity lastEntity = m_IndexToEntity[lastIndex];
        m_EntityToIndex[lastEntity] = removedIndex;
        m_IndexToEntity[removedIndex] = lastEntity;
        m_EntityToIndex.erase(entity);
        --m_Size;
    }

    bool HasData(Entity entity) const { return m_EntityToIndex.count(entity) > 0; }

    T& GetData(Entity entity)
    {
        assert(HasData(entity) && "Retrieving non-existent component.");
        return m_Components[m_EntityToIndex[entity]];
    }

    void EntityDestroyed(Entity entity) override
    {
        if (HasData(entity))
            RemoveData(entity);
    }

  private:
    std::array<T, MAX_ENTITIES> m_Components{};
    std::unordered_map<Entity, std::size_t> m_EntityToIndex;
    std::array<Entity, MAX_ENTITIES> m_IndexToEntity{};
    std::size_t m_Size{};
};
