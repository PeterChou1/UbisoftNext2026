#include "WinScreen.h"

#include "ECSManager.h"
#include "GameManager.h"
#include "Widget.h"
#include "stdafx.h"

extern ECSManager ECS;
extern GameManager GameSceneManager;

void WinScreen::Start()
{
    m_uistate = ECS.GetResource<UIState>();
    m_gamestate = ECS.GetResource<GameState>();
}

void WinScreen::Render()
{
    // Example screen size
    int screenWidth = APP_VIRTUAL_WIDTH;
    int screenHeight = APP_VIRTUAL_HEIGHT;

    // Container for the win screen
    float containerWidth = 300.0f;
    float containerHeight = 220.0f; // a bit taller to accommodate winner label
    float containerX = (screenWidth - containerWidth) * 0.5f;
    float containerY = (screenHeight - containerHeight) * 0.5f;

    DrawContainer((int)containerX, (int)containerY, containerWidth, containerHeight);

    // "Play Again" button
    float buttonWidth = 120.0f;
    float buttonHeight = 30.0f;
    float playAgainX = containerX + 30.0f;
    float playAgainY = containerY + containerHeight - buttonHeight - 10.0f;

    if (Button(1 /*unique ID*/,
               playAgainX,
               playAgainY,
               *m_uistate,
               buttonWidth,
               buttonHeight,
               "Title Screen"))
    {
        m_gamestate->ResetResource();
        m_gamestate->CurCameraState = StartMenu;
        GameSceneManager.SetActiveScene("TitleScreen");
    }

    // "Quit" button
    float quitX = containerX + containerWidth - buttonWidth - 30.0f;
    float quitY = playAgainY;
    if (Button(2 /*unique ID*/, quitX, quitY, *m_uistate, buttonWidth, buttonHeight, "Quit"))
        exit(0);

    // You lose screen
    float loseX = containerX + containerWidth - buttonWidth - 75.0f;
    float loseY = playAgainY - 100.0f;
    App::Print(loseX, loseY, "You Lose");
}
