//---------------------------------------------------------------------------------
// EntityManager.h
//---------------------------------------------------------------------------------
//
// Manages All Entity in the ECS System
// An Entity Manager maps an Entity to their Signature
// which denotes which component they own
//

#pragma once
#include "Entity.h"

#include <array>
#include <cassert>
#include <queue>
#include <set>
#include <string>
#include <vector>

class EntityManager
{
  public:
    EntityManager()
    {
        for (Entity entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            m_AvailableEntities.push(entity);
        }
    }

    Entity CreateEntity()
    {
        assert(m_LivingEntityCount < MAX_ENTITIES && "Too many entities in existence.");

        Entity id = m_AvailableEntities.front();
        m_AvailableEntities.pop();
        m_Alive[id] = true;
        ++m_LivingEntityCount;

        return id;
    }

    size_t GetEntityCount() { return m_LivingEntityCount; }

    void DestroyEntity(Entity entity)
    {
        assert(entity < MAX_ENTITIES && "Entity out of range.");
        m_Signatures[entity].reset();
        m_Alive[entity] = false;
        m_RecentlyDeleted.push_back(entity);
        m_AvailableEntities.push(entity);
        --m_LivingEntityCount;
    }

    void SetSignature(Entity entity, Signature signature)
    {
        assert(entity < MAX_ENTITIES && "Entity out of range.");

        m_Signatures[entity] = signature;
    }

    Signature GetSignature(Entity entity)
    {
        assert(entity < MAX_ENTITIES && "Entity out of range.");

        return m_Signatures[entity];
    }

    std::set<Entity> MatchSignature(Signature signature)
    {
        std::set<Entity> entities;

        for (Entity entity = 0; entity < MAX_ENTITIES; ++entity)
        {
            if ((m_Signatures[entity] & signature) == signature)
            {
                entities.insert(entity);
            }
        }

        return entities;
    }

    void Clear()
    {
        // clear queue
        std::queue<Entity> empty;
        std::swap(m_AvailableEntities, empty);
        for (Entity entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            m_AvailableEntities.push(entity);
        }
        for (Signature& signature : m_Signatures)
        {
            signature.reset();
        }
        m_Alive.fill(false);
        m_LivingEntityCount = 0;
    }

    void FlushRecentlyDeleted() { m_RecentlyDeleted.clear(); }

    bool IsAlive(Entity entity) const { return entity < MAX_ENTITIES && m_Alive[entity]; }

    /**
     * \brief All living entities in ascending order (used by the save system)
     */
    std::vector<Entity> GetLivingEntities() const
    {
        std::vector<Entity> living;
        living.reserve(m_LivingEntityCount);
        for (Entity entity = 1; entity < MAX_ENTITIES; ++entity)
        {
            if (m_Alive[entity])
                living.push_back(entity);
        }
        return living;
    }

    /**
     * \brief The free entity queue in the order CreateEntity will hand the IDs out.
     *        Saving this makes Entity allocation after a load identical to the
     *        allocation that would have happened in the original session
     */
    std::vector<Entity> GetAvailableEntities() const
    {
        std::vector<Entity> available;
        available.reserve(m_AvailableEntities.size());
        std::queue<Entity> copy = m_AvailableEntities;
        while (!copy.empty())
        {
            available.push_back(copy.front());
            copy.pop();
        }
        return available;
    }

    /**
     * \brief Check that a living / available pair describes a valid allocator:
     *        every id in [1, MAX_ENTITIES) must appear exactly once in either list
     */
    static bool ValidateState(const std::vector<Entity>& living,
                              const std::vector<Entity>& available,
                              std::string& error)
    {
        std::vector<bool> seen(MAX_ENTITIES, false);
        auto visit = [&](const std::vector<Entity>& ids, const char* listName) {
            for (Entity entity : ids)
            {
                if (entity == NULL_ENTITY || entity >= MAX_ENTITIES)
                {
                    error = std::string("Entity id out of range in ") + listName + " list: " +
                            std::to_string(entity);
                    return false;
                }
                if (seen[entity])
                {
                    error = "Entity id listed more than once: " + std::to_string(entity);
                    return false;
                }
                seen[entity] = true;
            }
            return true;
        };
        if (!visit(living, "living") || !visit(available, "available"))
            return false;
        if (living.size() + available.size() != MAX_ENTITIES - 1)
        {
            error = "Entity allocator state does not cover every entity id";
            return false;
        }
        return true;
    }

    /**
     * \brief Restore the allocator to an exact previously saved state.
     *        All signatures are cleared, components are expected to be re-added
     */
    void Restore(const std::vector<Entity>& living, const std::vector<Entity>& available)
    {
        std::string error;
        assert(ValidateState(living, available, error) && "Invalid entity allocator state");
        (void)error;
        Clear();
        std::queue<Entity> queue;
        for (Entity entity : available)
            queue.push(entity);
        std::swap(m_AvailableEntities, queue);
        for (Entity entity : living)
            m_Alive[entity] = true;
        m_LivingEntityCount = static_cast<uint32_t>(living.size());
        m_RecentlyDeleted.clear();
    }

  private:
    std::vector<Entity> m_RecentlyDeleted;
    std::queue<Entity> m_AvailableEntities{};
    std::array<Signature, MAX_ENTITIES> m_Signatures{};
    std::array<bool, MAX_ENTITIES> m_Alive{};
    uint32_t m_LivingEntityCount{};
};
