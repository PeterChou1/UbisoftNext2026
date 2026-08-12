#include "MainLevelUI.h"

#include "ECSManager.h"
#include "FragShaderTag.h"
#include "PlayerBase.h"
#include "PlayerUnits.h"
#include "RigidBody.h"
#include "Utils.h"
#include "Widget.h"
#include "app.h"
#include "stdafx.h"

extern ECSManager ECS;

MainLevelUI::MainLevelUI()
{
    m_GameState = ECS.GetResource<GameState>();
    m_UIState = ECS.GetResource<UIState>();
    m_Cam = ECS.GetResource<Camera>();
}

void MainLevelUI::Update()
{
    // Don't bother handling logic building obstacles
    if (m_UIState->state == BuildObstacleContext)
        return;
    // Handle Bringing up context menus
    Entity Base = *ECS.Visit<PlayerBaseComponent>().begin();
    FragShaderTag& Frag = ECS.GetComponent<FragShaderTag>(Base);
    float mouseX = m_UIState->mouseX;
    float mouseY = m_UIState->mouseY;
    bool mouseCollide = Utils::MousePointMeshIntersect(mouseX, mouseY, *m_Cam, Base);

    if (mouseCollide)
        Frag.FragAssetId = RedShaderID;
    else
        Frag.FragAssetId = BlinnPhongID;

    if (mouseCollide && m_UIState->leftClick)
        m_UIState->state = InBaseContextMenu;
}

void MainLevelUI::Render()
{
    if (m_UIState->state == BuildObstacleContext)
        RenderObstacleContext();
    else if (m_UIState->state == InBaseContextMenu)
        RenderBaseContext();

    // Render your base health
    Entity Base = *ECS.Visit<PlayerBaseComponent>().begin();
    PlayerBaseComponent& B = ECS.GetComponent<PlayerBaseComponent>(Base);
    FillBar(100, APP_VIRTUAL_HEIGHT - 100, 300, 50, B.PlayerBaseHealth);
    // Render
    std::string CrystalCount =
            "Crystal Count: " + std::to_string(m_GameState->PlayerCrystalInventory);
    App::Print(450, APP_VIRTUAL_HEIGHT - 75, CrystalCount.c_str());
    std::string Label = "";
    if (m_GameState->currentState == Prepartion)
    {
        Label = "Preparation Phase: " + std::to_string(m_GameState->CurrentTimePrep);
    }
    else if (m_GameState->currentState == Invasion)
    {
        if (m_GameState->CurrentTimeInvasion > 0)
            Label = "Invasion Phase Spawn Period: " +
                    std::to_string(m_GameState->CurrentTimeInvasion);
        else
            Label = "Invasion Phase (Kill All Enemy to Advance)";
    }
    App::Print(600, APP_VIRTUAL_HEIGHT - 75, Label.c_str());
    std::string RoundLabel = "Round: " + std::to_string(m_GameState->RoundNumber);
    App::Print(100, 75, RoundLabel.c_str());
    std::string InstructionLabel = "WASD to pan camera - LEFT Click to select/guide units - RIGHT "
                                   "Click to unselect - SPACE to group units";
    App::Print(100, 25, InstructionLabel.c_str());
}

void MainLevelUI::RenderBaseContext()
{
    float width = 400;
    float height = 400;
    float containerX = APP_VIRTUAL_WIDTH / 2 - (width / 2);
    float containerY = APP_VIRTUAL_HEIGHT / 2 - (height / 2);

    // Render Container as background
    DrawContainer(containerX, containerY, width, height);

    float offsetBack = 40;
    // "Back" button
    if (Button(1, containerX + offsetBack, containerY + offsetBack, *m_UIState, 140, 25, "Back"))
    {
        m_UIState->activeItem = 0;
        m_UIState->hotItem = 0;
        m_UIState->state = DefaultContext;
    }

    float offsetButtons = 100;
    // Common spacing parameters
    float startY = containerY + height - offsetButtons;
    float stepY = 20.0f;
    float stepYButton = 40.0f;
    float labelX = 90.0f;
    float dropX = containerX + offsetButtons;
    float dropW = 200.0f;
    float dropH = 30.0f;

    if (Button(2, dropX, startY, *m_UIState, dropW, dropH, "Purchase Soldiers"))
    {
        m_UIState->activeItem = 0;
        m_UIState->hotItem = 0;

        if (m_GameState->PlayerCrystalInventory >= 10)
        {
            m_UIState->state = DefaultContext;
            m_GameState->PlayerCrystalInventory -= 10;
            CreateSoldierBattalion(0, -3, 100, 5, m_GameState->battalionCount);
            m_GameState->battalionCount++;
        }
        else
            App::PlayAudio("data/Sounds/wrongSound.wav");
    }

    // Move down for the next item
    startY -= stepY;

    std::string Label = "Cost 10 crystals";
    App::Print(dropX, startY, Label.c_str());

    startY -= stepYButton;

    if (Button(3, dropX, startY, *m_UIState, dropW, dropH, "Purchase Support"))
    {
        m_UIState->activeItem = 0;
        m_UIState->hotItem = 0;

        if (m_GameState->PlayerCrystalInventory >= 15)
        {
            m_GameState->PlayerCrystalInventory -= 15;
            m_UIState->state = DefaultContext;
            CreateSupportBattalion(0, -3, 100, 5, m_GameState->battalionCount);
            m_GameState->battalionCount++;
        }
        else
            App::PlayAudio("data/Sounds/wrongSound.wav");
    }

    startY -= stepY;

    Label = "Cost 15 crystals";
    App::Print(dropX, startY, Label.c_str());

    startY -= stepYButton;

    if (Button(4, dropX, startY, *m_UIState, dropW, dropH, "Purchase Tank"))
    {
        m_UIState->activeItem = 0;
        m_UIState->hotItem = 0;
        if (m_GameState->PlayerCrystalInventory >= 50)
        {
            m_GameState->PlayerCrystalInventory -= 50;
            m_UIState->state = DefaultContext;
            CreateTank(0, -3, m_GameState->battalionCount);
            m_GameState->battalionCount++;
        }
        else
            App::PlayAudio("data/Sounds/wrongSound.wav");
    }

    startY -= stepY;

    Label = "Cost 50 crystals";
    App::Print(dropX, startY, Label.c_str());

    startY -= stepYButton;

    if (Button(5, dropX, startY, *m_UIState, dropW, dropH, "Purchase Obstacle"))
    {
        m_UIState->activeItem = 0;
        m_UIState->hotItem = 0;
        if (m_GameState->PlayerCrystalInventory >= 10)
        {
            m_GameState->PlayerCrystalInventory -= 10;
            m_UIState->state = BuildObstacleContext;
        }
        else
            App::PlayAudio("data/Sounds/wrongSound.wav");
    }

    startY -= stepY;

    Label = "Cost 10 crystals";
    App::Print(dropX, startY, Label.c_str());
}

void MainLevelUI::RenderObstacleContext()
{
    float mouseX = m_UIState->mouseX;
    float mouseY = m_UIState->mouseY;
    std::string Label = "Press R to Rotate | Click to Place";
    App::Print(mouseX, mouseY, Label.c_str());
}
