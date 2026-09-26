//---------------------------------------------------------------------------------
// EditorMain.cpp
//---------------------------------------------------------------------------------
//
// Entry points of the SceneEditor program (called by the ContestAPI main loop).
// The editor links the project's scripts (GameScripts) so it can offer them in
// the inspector and run them in play mode.
//
#include "stdafx.h"
//------------------------------------------------------------------------
#include "ECSManager.h"
#include "GameManager.h"
#include "GameScripts.h"
#include "SceneEditorScene.h"
#include "UIText.h"
#include "app.h"
#include "main.h"

#include <memory>

extern ECSManager ECS;
extern GameManager GameSceneManager;

void Init()
{
    // The UI measures its text with GLUT's own font metrics (UIText.h)
    UIText::SetMeasure([](const std::string& text) {
        int pixels = 0;
        for (unsigned char c : text)
            pixels += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, c);
        return pixels;
    });
    ECS.Init();
    GameSceneManager.Setup();
    RegisterGameScripts();
    GameSceneManager.RegisterScene("SceneEditor", std::make_unique<SceneEditorScene>());
    GameSceneManager.SetActiveScene("SceneEditor");
}

void Update(float deltaTime)
{
    // Keeps labels centered and inside their widgets when the window is
    // resized (text is drawn in pixels, the UI in virtual units)
    UIText::SetWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    GameSceneManager.Update(deltaTime);
}

void Render()
{
    GameSceneManager.Render();
    ECS.FlushECS();
}

void Shutdown() {}
