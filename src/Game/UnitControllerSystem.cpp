#include "UnitControllerSystem.h"

#include "ECSManager.h"
#include "FragShaderTag.h"
#include "PlayerUnits.h"
#include "RigidBody.h"
#include "UITarget.h"
#include "app.h"

extern ECSManager ECS;

UnitControllerSystem::UnitControllerSystem()
{
    m_Cam = ECS.GetResource<Camera>();
    m_UIstate = ECS.GetResource<UIState>();
    m_GameState = ECS.GetResource<GameState>();
    m_Board = ECS.GetResource<BlackBoard>();
}

void UnitControllerSystem::DeleteDeadUnits()
{
    for (Entity E : ECS.Visit<PlayerControlUnit>())
    {
        PlayerControlUnit& unit = ECS.GetComponent<PlayerControlUnit>(E);
        if (unit.health < 0)
            ECS.DestroyEntity(E);
    }
}

void UnitControllerSystem::HandleUnitSelection()
{
    // Do not select units
    if (m_UIstate->state == BuildObstacleContext)
        return;

    float mouseX = m_UIstate->mouseX;
    float mouseY = m_UIstate->mouseY;
    Entity UnitSelected = NULL_ENTITY;

    // Right click to clear selected unit
    if (m_UIstate->rightClick)
    {
        ClearSelected();
        return;
    }

    bool MadeSelection = false;
    bool MergeSelection = false;

    for (Entity E : ECS.Visit<PlayerControlUnit, RigidBody>())
    {
        bool mouseCollide = Utils::MousePointMeshIntersect(mouseX, mouseY, *m_Cam, E);
        if (mouseCollide && m_UIstate->leftClick)
        {
            if (!App::IsKeyPressed(App::KEY_SPACE))
                ClearSelected();
            else
                MergeSelection = true;

            UnitSelected = E;
            MadeSelection = true;
            break;
        }
    }

    if (UnitSelected != NULL_ENTITY)
    {
        auto& Unit = ECS.GetComponent<PlayerControlUnit>(UnitSelected);

        if (Unit.isWall)
            return;

        int selectId = Unit.battalionId;
        for (Entity E : ECS.Visit<PlayerControlUnit>())
        {
            PlayerControlUnit& soldier = ECS.GetComponent<PlayerControlUnit>(E);
            int curId = soldier.battalionId;
            if (curId == selectId)
                soldier.selected = true;
        }
        // Merge selected entity
        if (MergeSelection)
        {
            for (Entity E : ECS.Visit<PlayerControlUnit>())
            {
                PlayerControlUnit& soldier = ECS.GetComponent<PlayerControlUnit>(E);
                if (soldier.selected)
                    soldier.battalionId = selectId;
            }
        }
    }
    bool HasSelected = false;

    // Shade every selected unit red
    for (Entity E : ECS.Visit<PlayerControlUnit, FragShaderTag>())
    {
        FragShaderTag& F = ECS.GetComponent<FragShaderTag>(E);
        PlayerControlUnit& soldier = ECS.GetComponent<PlayerControlUnit>(E);
        if (soldier.selected)
        {
            HasSelected = true;
            F.FragAssetId = RedShaderID;
        }
        else
            F.FragAssetId = BlinnPhongID;
    }

    Entity UIEntity = *ECS.Visit<UITarget, Transform>().begin();
    UITarget& target = ECS.GetComponent<UITarget>(UIEntity);
    Transform& T = ECS.GetComponent<Transform>(UIEntity);
    // User clicked a part of the board direct all unit towards it
    if (HasSelected && !MadeSelection && m_UIstate->leftClick)
    {
        Vec3 planePt = Vec3(0, 0, 0);
        Vec3 planeNormal = Vec3(0, 1, 0);
        Vec3 worldPoint = m_Cam->ScreenSpaceToWorldPoint(mouseX, mouseY, planePt, planeNormal);
        T.SetLocalPosition(worldPoint);
        target.active = true;
    }

    if (!HasSelected)
    {
        T.SetLocalPosition(m_GameState->OffscreenPosition);
        target.active = false;
    }
    // Calculate Vector Fields to point towards user selection
    m_Board->UnitVectorField.CalculateVectorField(T);
}

void UnitControllerSystem::ClearSelected()
{
    for (Entity E : ECS.Visit<PlayerControlUnit>())
    {
        PlayerControlUnit& soldier = ECS.GetComponent<PlayerControlUnit>(E);
        soldier.selected = false;
    }
}

void UnitControllerSystem::Update()
{
    DeleteDeadUnits();
    // Handle Selecting/Grouping/Moving Units
    HandleUnitSelection();
}
