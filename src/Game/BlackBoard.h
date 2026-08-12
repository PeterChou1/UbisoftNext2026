#pragma once

#include "BehaviorTree.h"
#include "Entity.h"
#include "Map.h"
#include "Resource.h"
#include "Transform.h"

#include <unordered_map>

// The BlackBoard is a global singleton resource which stores all behavior tree
// plus any shared data for btree nodes to access
struct BlackBoard : Resource
{

    Entity UnitTarget;
    VectorField UnitVectorField;

    Entity EnemyTarget;
    VectorField EnemyVectorField;

    std::unordered_map<Entity, Entity> PlayerTankTargets;
    std::unordered_map<Entity, Entity> EnemyTankTargets;
    // std::unordered_map<Entity, > NotInLineOfSight;
    std::unordered_map<Entity, Vec3> LastKnownLocation;
    std::unordered_map<Entity, bool> InLineOfSight;
    std::unordered_map<Entity, Vec3> PatrolTargets;
    std::unordered_map<Entity, std::shared_ptr<Node>> BehaviorTreeDataBase;

    float DeltaTime;

    void ClearAIMemory()
    {
        PlayerTankTargets.clear();
        LastKnownLocation.clear();
        InLineOfSight.clear();
        PatrolTargets.clear();
    }

    void SetTarget(Entity P) { UnitTarget = P; }

    void SetMapObstacles(std::set<Entity>& Obstacles);

    void RemoveMapObstacle(std::set<Entity>& Obstacles);

    void SetMapLocation(Vec3& Location);

    void AddBehaviouralTree(Entity entity, std::shared_ptr<Node> node);

    void ResetResource() override
    {
        ClearAIMemory();
        BehaviorTreeDataBase.clear();
        PatrolTargets.clear();
        UnitVectorField.ClearField();
    }
};
