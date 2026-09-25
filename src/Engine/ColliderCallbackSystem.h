#pragma once
#include "ColliderCategory.h"
#include "Resource.h"
#include "RigidBody.h"

#include <set>
#include <unordered_map>
#include <vector>

/**
 * \brief A contact between two bodies starting or ending (every body pair,
 *        independent of the category callbacks). Consumed by the ScriptSystem
 */
struct ContactEvent
{
    enum Kind
    {
        Enter,
        Exit
    };
    Kind Type;
    Entity A;
    Entity B;
};

class ColliderCallbackSystem : public Resource
{
  public:
    ColliderCallbackSystem() = default;

    void RegisterCallback(const std::shared_ptr<Collider>& callback);

    void ResetResource() override;

    bool HasRegisterCallback(CollisionPair pair);

    /**
     * \brief Any category callback registered (they may read transforms
     *        between physics sub steps)
     */
    bool HasCallbacks() const { return !m_CallBackMap.empty(); }

    void SubmitForCallback(Entity A, Entity B);

    void Update();

    /**
     * \brief Report that two bodies touch during the current physics step
     */
    void SubmitContact(Entity A, Entity B);

    /**
     * \brief Contact enter / exit events since the last call (then cleared)
     */
    std::vector<ContactEvent> TakeContactEvents();

  private:
    void UpdateContacts();

    // Pairs touching this sub step (unsorted, may repeat) and last sub step
    // (sorted, unique): sorted vectors instead of std::set, this runs for
    // every contact every physics sub step
    std::vector<std::pair<Entity, Entity>> m_Contacts;
    std::vector<std::pair<Entity, Entity>> m_PrevContacts;
    std::vector<ContactEvent> m_ContactEvents;

    std::set<std::pair<Entity, Entity>> m_CollidePairs;
    std::set<std::pair<Entity, Entity>> m_PrevCollidePairs;

    std::unordered_map<CollisionPair, std::shared_ptr<Collider>, CollisionPairHash> m_CallBackMap;
};
