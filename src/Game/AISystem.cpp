#include "AISystem.h"

#include "BasicEnemyUnit.h"
#include "ECSManager.h"
#include "PlayerUnits.h"
#include "stdafx.h"

extern ECSManager ECS;

AISystem::AISystem()
{
    m_BlackBoard = ECS.GetResource<BlackBoard>();
}

void AISystem::Update()
{
    auto& BTreeDB = m_BlackBoard->BehaviorTreeDataBase;

    for (Entity e : ECS.Visit<BehaviorTree>())
    {
        assert(BTreeDB.count(e) > 0 && "Behavior Tree not registered");
        BTreeDB[e]->Process();
    }

    for (Entity e : ECS.VisitDeleted<BehaviorTree>())
    {
        BTreeDB.erase(e);
    }

    // Add Obstacles
    std::set<Entity> ObstacleAddSet = ECS.Visit<AIObstacle>();
    if (m_Obstacles != ObstacleAddSet)
    {
        m_Obstacles = ObstacleAddSet;
        m_BlackBoard->SetMapObstacles(ObstacleAddSet);
    }
    // Remove Obstacles
    std::set<Entity> ObstacleDeleted = ECS.VisitDeleted<AIObstacle>();
    m_BlackBoard->RemoveMapObstacle(ObstacleDeleted);
}
