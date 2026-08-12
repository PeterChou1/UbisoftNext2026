#include "BuildObstaclesSystem.h"

#include "ECSManager.h"
#include "FragShaderTag.h"
#include "GameUtils.h"
#include "PlayerUnits.h"
#include "RigidBody.h"
#include "app.h"

extern ECSManager ECS;

Entity CreateBuildObstacles(Vec3& Location)
{
    Entity Unit = CreateMeshEntity(Location, ObstacleWall);
    auto rigidbody = RigidBody(1.0f, 10.0f);
    rigidbody.Category = UnitCollider;
    ECS.AddComponent<RigidBody>(Unit, rigidbody);
    ECS.AddComponent<PlayerControlUnit>(Unit, {-1, 250, false});
    ECS.AddComponent<FragShaderTag>(Unit, FragShaderTag(BlinnPhongID));

    return Unit;
}

BuildObstaclesSystem::BuildObstaclesSystem()
{
    m_Cam = ECS.GetResource<Camera>();
    m_UIstate = ECS.GetResource<UIState>();
    m_GameState = ECS.GetResource<GameState>();
}

void BuildObstaclesSystem::Update(float deltaTime)
{
    if (m_UIstate->state != BuildObstacleContext)
        return;

    float mouseX = m_UIstate->mouseX;
    float mouseY = m_UIstate->mouseY;
    Vec3 planePt = Vec3(0, 0, 0);
    Vec3 planeNormal = Vec3(0, 1, 0);
    Vec3 worldPoint = m_Cam->ScreenSpaceToWorldPoint(mouseX, mouseY, planePt, planeNormal);

    if (m_GameState->ObstacleInCursor == NULL_ENTITY)
        m_GameState->ObstacleInCursor = CreateBuildObstacles(worldPoint);

    Transform& T = ECS.GetComponent<Transform>(m_GameState->ObstacleInCursor);
    RigidBody& Rb = ECS.GetComponent<RigidBody>(m_GameState->ObstacleInCursor);
    FragShaderTag& shader = ECS.GetComponent<FragShaderTag>(m_GameState->ObstacleInCursor);
    T.SetLocalPosition(worldPoint);

    if (App::IsKeyPressed(App::KEY_R))
    {
        m_UIstate->flipped = !m_UIstate->flipped;
        Quat Q = T.GetWorldRotation();
        Quat q90({0, 1, 0}, PI * 0.5f);
        Q *= q90;
        T.SetGlobalRotation(Q);
    }

    if (Rb.IsIntersecting)
        shader.FragAssetId = RedShaderID;
    else
        shader.FragAssetId = BlinnPhongID;

    if (m_UIstate->leftClick)
    {
        // Fail
        if (Rb.IsIntersecting)
        {
            App::PlayAudio("data/Sounds/wrongSound.wav");
            return;
        }

        App::PlayAudio("data/Sounds/clickSound.wav");
        AIObstacle A;
        if (m_UIstate->flipped)
            A = {5, 1};
        else
            A = {1, 5};

        ECS.AddComponent<AIObstacle>(m_GameState->ObstacleInCursor, A);
        Rb.SetStatic();
        m_UIstate->state = DefaultContext;
        m_UIstate->flipped = false;
        m_GameState->ObstacleInCursor = NULL_ENTITY;
    }
}
