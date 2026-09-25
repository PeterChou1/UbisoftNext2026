//---------------------------------------------------------------------------------
// ColliderCallbackSystem.h
//---------------------------------------------------------------------------------
//
// Calls the Collider callbacks registered for pairs of collision categories
// (enter / stay / exit), and records the contact events of every body pair
//
#pragma once

#include "ColliderCategory.h"
#include "Resource.h"
#include "RigidBody.h"

#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
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
    void RegisterCallback(const std::shared_ptr<Collider>& callback);

    void ResetResource() override;

    /**
     * \brief Whether a callback is registered for the categories, in either order
     */
    bool HasRegisterCallback(CollisionPair pair);

    /**
     * \brief Any category callback registered (they may read transforms
     *        between physics sub steps)
     */
    bool HasCallbacks() const { return !m_CallBackMap.empty(); }

    /**
     * \brief Report that two bodies with a registered callback touch during
     *        the current physics sub step
     */
    void SubmitForCallback(Entity A, Entity B);

    /**
     * \brief After every physics sub step: the contact events, then the
     *        callbacks of the submitted pairs
     */
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

    /**
     * \brief Call event(callback, self, other, selfBody, otherBody) with the
     *        callback registered for the two bodies' categories (the bodies in
     *        its order). False when there is none
     */
    template <typename Fn>
    bool Dispatch(Entity e1, Entity e2, Fn&& event);

    // Pairs touching this sub step (unsorted, may repeat) and last sub step
    // (sorted, unique): sorted vectors instead of std::set, this runs for
    // every contact every physics sub step
    std::vector<std::pair<Entity, Entity>> m_Contacts;
    std::vector<std::pair<Entity, Entity>> m_PrevContacts;
    std::vector<ContactEvent> m_ContactEvents;

    // Pairs submitted for a callback this sub step and last sub step
    std::set<std::pair<Entity, Entity>> m_CollidePairs;
    std::set<std::pair<Entity, Entity>> m_PrevCollidePairs;

    std::unordered_map<CollisionPair, std::shared_ptr<Collider>, CollisionPairHash> m_CallBackMap;
};
