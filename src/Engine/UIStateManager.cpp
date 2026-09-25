#include "UIStateManager.h"

#include "ECSManager.h"
#include "app.h"

extern ECSManager ECS;

UIStateManager::UIStateManager()
{
    state = ECS.GetResource<UIState>();
}

void UIStateManager::Update()
{
    float mouseX, mouseY;
    App::GetMousePos(mouseX, mouseY);
    state->mouseX = mouseX;
    state->mouseY = APP_VIRTUAL_HEIGHT - mouseY;
    state->mouseLeftDown = App::IsMousePressed(GLUT_LEFT_BUTTON);
    state->mouseRightDown = App::IsMousePressed(GLUT_RIGHT_BUTTON);

    if (!state->mouseLeftDownPrevFrame && state->mouseLeftDown)
        state->leftClick = true;

    if (!state->mouseRightDownPrevFrame && state->mouseRightDown)
        state->rightClick = true;

    state->hotItem = -1;
    state->activeItem = -1;
}

void UIStateManager::CleanUp()
{
    state->mouseLeftDownPrevFrame = state->mouseLeftDown;
    state->mouseRightDownPrevFrame = state->mouseRightDown;

    state->leftClick = false;
    state->rightClick = false;
}
