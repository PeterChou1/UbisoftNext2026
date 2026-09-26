#include "ColliderCallbackSystem.h"

#include "ECSManager.h"
#include "stdafx.h"

#include <algorithm>
#include <cassert>

extern ECSManager ECS;

void ColliderCallbackSystem::RegisterCallback(const std::shared_ptr<Collider>& callback)
{
    m_CallBackMap[callback->GetCollisionPair()] = callback;
}

void ColliderCallbackSystem::ResetResource()
{
    m_CallBackMap.clear();
    m_Contacts.clear();
    m_PrevContacts.clear();
    m_ContactEvents.clear();
}

void ColliderCallbackSystem::SubmitContact(Entity A, Entity B)
{
    // Store every pair in one orientation (smallest id first)
    m_Contacts.push_back(A < B ? std::make_pair(A, B) : std::make_pair(B, A));
}

std::vector<ContactEvent> ColliderCallbackSystem::TakeContactEvents()
{
    std::vector<ContactEvent> events;
    std::swap(events, m_ContactEvents);
    return events;
}

void ColliderCallbackSystem::UpdateContacts()
{
    std::sort(m_Contacts.begin(), m_Contacts.end());
    m_Contacts.erase(std::unique(m_Contacts.begin(), m_Contacts.end()), m_Contacts.end());
    // New pairs, then pairs that stopped touching, each in pair order
    auto previous = m_PrevContacts.begin();
    for (const auto& pair : m_Contacts)
    {
        while (previous != m_PrevContacts.end() && *previous < pair)
            ++previous;
        if (previous == m_PrevContacts.end() || *previous != pair)
            m_ContactEvents.push_back({ContactEvent::Enter, pair.first, pair.second});
    }
    auto current = m_Contacts.begin();
    for (const auto& pair : m_PrevContacts)
    {
        while (current != m_Contacts.end() && *current < pair)
            ++current;
        if (current == m_Contacts.end() || *current != pair)
            m_ContactEvents.push_back({ContactEvent::Exit, pair.first, pair.second});
    }
    m_PrevContacts.swap(m_Contacts);
    m_Contacts.clear();
}

bool ColliderCallbackSystem::HasRegisterCallback(CollisionPair pair)
{
    return m_CallBackMap.count(pair) > 0 || m_CallBackMap.count({pair.second, pair.first}) > 0;
}

void ColliderCallbackSystem::SubmitForCallback(Entity A, Entity B)
{
    // Each pair once, in the orientation first submitted
    if (m_CollidePairs.count({B, A}) == 0)
        m_CollidePairs.insert({A, B});
}

template <typename Fn>
bool ColliderCallbackSystem::Dispatch(Entity e1, Entity e2, Fn&& event)
{
    RigidBody& A = ECS.GetComponent<RigidBody>(e1);
    RigidBody& B = ECS.GetComponent<RigidBody>(e2);
    auto callback = m_CallBackMap.find({A.Category, B.Category});
    if (callback != m_CallBackMap.end())
    {
        event(*callback->second, e1, e2, A, B);
        return true;
    }
    callback = m_CallBackMap.find({B.Category, A.Category});
    if (callback != m_CallBackMap.end())
    {
        event(*callback->second, e2, e1, B, A);
        return true;
    }
    return false;
}

void ColliderCallbackSystem::Update()
{
    UpdateContacts();

    // Callbacks may destroy bodies: the pairs of destroyed bodies are skipped
    std::set<Entity> deleted = ECS.VisitDeleted<RigidBody>();
    auto isDeleted = [&](const std::pair<Entity, Entity>& pair) {
        return deleted.count(pair.first) > 0 || deleted.count(pair.second) > 0;
    };

    for (const auto& pair : m_CollidePairs)
    {
        if (isDeleted(pair))
            continue;
        assert(ECS.HasComponent<RigidBody>(pair.first) && "Not Possible");
        assert(ECS.HasComponent<RigidBody>(pair.second) && "Not Possible");

        const bool touching = m_PrevCollidePairs.count(pair) > 0;
        if (Dispatch(pair.first,
                     pair.second,
                     [&](Collider& callback,
                         Entity self,
                         Entity other,
                         RigidBody& selfBody,
                         RigidBody& otherBody) {
                         if (touching)
                             callback.OnCollide(self, other, selfBody, otherBody);
                         else
                             callback.OnCollideEnter(self, other, selfBody, otherBody);
                     }))
            deleted = ECS.VisitDeleted<RigidBody>();
    }

    for (const auto& pair : m_PrevCollidePairs)
    {
        if (isDeleted(pair) || m_CollidePairs.count(pair) > 0)
            continue;
        if (!ECS.HasComponent<RigidBody>(pair.first) || !ECS.HasComponent<RigidBody>(pair.second))
            continue;
        if (Dispatch(pair.first,
                     pair.second,
                     [](Collider& callback,
                        Entity self,
                        Entity other,
                        RigidBody& selfBody,
                        RigidBody& otherBody) {
                         callback.OnCollideExit(self, other, selfBody, otherBody);
                     }))
            deleted = ECS.VisitDeleted<RigidBody>();
    }

    // This sub step's pairs (of bodies still alive) become the previous ones
    m_PrevCollidePairs.clear();
    for (const auto& pair : m_CollidePairs)
    {
        if (!isDeleted(pair))
            m_PrevCollidePairs.insert(pair);
    }
    m_CollidePairs.clear();
}
