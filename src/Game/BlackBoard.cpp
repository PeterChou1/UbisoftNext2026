#include "BlackBoard.h"

#include "Camera.h"
#include "ECSManager.h"
#include "stdafx.h"

extern ECSManager ECS;

void BlackBoard::SetMapObstacles(std::set<Entity>& Obstacles)
{
    UnitVectorField.SetObstacles(Obstacles);
    EnemyVectorField.SetObstacles(Obstacles);
}

void BlackBoard::RemoveMapObstacle(std::set<Entity>& Obstacles)
{
    UnitVectorField.RemoveObstacles(Obstacles);
    EnemyVectorField.RemoveObstacles(Obstacles);
}

void BlackBoard::SetMapLocation(Vec3& Location)
{
    UnitVectorField.CreateVectorField(Location);
}

void BlackBoard::AddBehaviouralTree(Entity entity, std::shared_ptr<Node> node)
{
    ECS.AddComponent<BehaviorTree>(entity, BehaviorTree());
    BehaviorTreeDataBase[entity] = node;
}
