//---------------------------------------------------------------------------------
// ECSManager.h
//---------------------------------------------------------------------------------
//
// The interface of the ECS (Entity Component System), which holds the global
// state of the game. Broadly speaking:
//   - Entities are containers for Components
//   - Components are data classes
//   - Systems act on the Components (they Visit the entities holding some)
//
// It also holds Resources: global objects created once per game
//
#pragma once

#include "ComponentManager.h"
#include "Entity.h"
#include "EntityManager.h"
#include "Resource.h"
#include "VisitorManager.h"

#include <cassert>
#include <memory>
#include <set>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

class ECSManager
{
  public:
    /**
     * \brief Initialized all subcomponents of the ECS system
     */
    void Init()
    {
        m_ComponentManager = std::make_unique<ComponentManager>();
        m_EntityManager = std::make_unique<EntityManager>();
        m_VisitorManager = std::make_unique<VisitorManager>();
    }

    size_t GetEntityCount() const { return m_EntityManager->GetEntityCount(); }

    /**
     * \brief Meant for internal use flushes deleted Entity list
     */
    void FlushECS() { m_VisitorManager->FlushDeletedEntities(); }

    /**
     * \brief Resets the ECS System by clearing everything
     */
    void Reset()
    {
        ClearWorld();
        for (const auto& resource : m_Resources)
            resource->ResetResource();
    }

    /**
     * \brief Removes every Entity, Component and Visitor but keeps all Resources
     *        intact (unlike Reset which also calls ResetResource). Used by the
     *        save system before restoring a saved world
     */
    void ClearWorld()
    {
        m_ComponentManager->Clear();
        m_EntityManager->Clear();
        m_VisitorManager->Clear();
    }

    /**
     * \brief Check whether an Entity is currently alive
     */
    bool IsEntityAlive(Entity entity) const { return m_EntityManager->IsAlive(entity); }

    /**
     * \brief All living Entity in ascending order
     */
    std::vector<Entity> GetLivingEntities() const { return m_EntityManager->GetLivingEntities(); }

    /**
     * \brief Free Entity ids in the order they will be handed out by CreateEntity
     */
    std::vector<Entity> GetAvailableEntities() const
    {
        return m_EntityManager->GetAvailableEntities();
    }

    /**
     * \brief Recreate an exact set of living Entity (without components) and the
     *        exact free list order. Any existing Entity/Component are removed
     *        first. Components must be re-added with AddComponent afterwards
     */
    void RestoreEntities(const std::vector<Entity>& living, const std::vector<Entity>& available)
    {
        ClearWorld();
        m_EntityManager->Restore(living, available);
    }

    /**
     * \brief Creates an Entity in the ECS System
     * \return an Entity ID
     */
    Entity CreateEntity() { return m_EntityManager->CreateEntity(); }

    /**
     * \brief Visit all entities that hold a particular Ts Components
     * \tparam Ts Component Types the entities have
     * \return all Entity that hold Ts data types
     */
    template <typename... Ts>
    std::set<Entity> Visit()
    {
        return GetVisitor<Ts...>().Entities;
    }

    /**
     * \brief Visit all entities that hold a particular set of Entity
     *        that were deleted by other Systems in this game loop
     *
     * \tparam Ts Component Types to Visit
     * \return All Entity that hold Ts data types that were deleted by
     *         other systems
     */
    template <typename... Ts>
    std::set<Entity> VisitDeleted()
    {
        return GetVisitor<Ts...>().DeletedEntities;
    }

    /**
     * \brief Remove the Entity from the ECS System
     */
    void DestroyEntity(Entity entity)
    {
        auto signature = m_EntityManager->GetSignature(entity);
        m_VisitorManager->EntitySignatureDeleted(entity, signature);
        m_EntityManager->DestroyEntity(entity);
        m_ComponentManager->EntityDestroyed(entity);
        m_VisitorManager->EntityDestroyed(entity);
    }

    /**
     * \brief Add Component T to Entity
     */
    template <typename T>
    void AddComponent(Entity entity, T component)
    {
        m_ComponentManager->AddComponent<T>(entity, std::move(component));
        auto signature = m_EntityManager->GetSignature(entity);
        signature.set(m_ComponentManager->GetComponentType<T>(), true);
        m_EntityManager->SetSignature(entity, signature);
        m_VisitorManager->EntitySignatureChanged(entity, signature);
    }

    /**
     * \brief Remove Component T from Entity
     */
    template <typename T>
    void RemoveComponent(Entity entity)
    {
        m_ComponentManager->RemoveComponent<T>(entity);
        auto signature = m_EntityManager->GetSignature(entity);
        m_VisitorManager->EntitySignatureDeleted(entity, signature);
        signature.set(m_ComponentManager->GetComponentType<T>(), false);
        m_EntityManager->SetSignature(entity, signature);
        m_VisitorManager->EntitySignatureChanged(entity, signature);
    }

    /**
     * \brief Check if Entity has Component T
     */
    template <typename T>
    bool HasComponent(Entity entity) const
    {
        return m_ComponentManager->HasComponent<T>(entity);
    }

    /**
     * \brief GetComponent T from Entity
     */
    template <typename T>
    T& GetComponent(Entity entity)
    {
        return m_ComponentManager->GetComponent<T>(entity);
    }

    /**
     * \brief Registered a Resource within the ECS System
     *        Note you cannot register a resource twice
     */
    template <typename T>
    void RegisterResource(T resource)
    {
        static_assert(std::is_base_of<Resource, T>::value, "T must derive from resource");
        const char* typeName = typeid(T).name();
        assert(m_ResourceToIndex.find(typeName) == m_ResourceToIndex.end() &&
               "Resource already registered");
        m_ResourceToIndex[typeName] = m_Resources.size();
        m_Resources.push_back(std::make_shared<T>(resource));
    }

    /**
     * \brief Get a Registered Resource T
     */
    template <typename T>
    std::shared_ptr<T> GetResource()
    {
        static_assert(std::is_base_of<Resource, T>::value, "T must derive from resource");
        const char* typeName = typeid(T).name();
        assert(m_ResourceToIndex.find(typeName) != m_ResourceToIndex.end() &&
               "Resource Not Registered");
        return std::dynamic_pointer_cast<T>(m_Resources[m_ResourceToIndex[typeName]]);
    }

    /**
     * \brief Check whether a Resource T was registered
     */
    template <typename T>
    bool HasResource() const
    {
        static_assert(std::is_base_of<Resource, T>::value, "T must derive from resource");
        return m_ResourceToIndex.find(typeid(T).name()) != m_ResourceToIndex.end();
    }

  private:
    /**
     * \brief The Visitor of Ts, registered (with its entities) on first use
     */
    template <typename... Ts>
    Visitor& GetVisitor()
    {
        if (Visitor* visitor = m_VisitorManager->Find<Ts...>())
            return *visitor;
        Signature requirements = m_ComponentManager->GetSignature<Ts...>();
        Visitor& visitor = m_VisitorManager->Register<Ts...>(requirements);
        visitor.Entities = m_EntityManager->MatchSignature(requirements);
        return visitor;
    }

    std::unique_ptr<ComponentManager> m_ComponentManager;
    std::unique_ptr<EntityManager> m_EntityManager;
    std::unique_ptr<VisitorManager> m_VisitorManager;
    std::unordered_map<const char*, size_t> m_ResourceToIndex;
    std::vector<std::shared_ptr<Resource>> m_Resources;
};
