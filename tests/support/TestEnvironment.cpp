#include "TestEnvironment.h"

#include "AppStub.h"

#include "ECSManager.h"
#include "GameManager.h"
#include "GameScripts.h"
#include "SceneEditorScene.h"
#include "SceneMenu.h"
#include "World/ScenePlayer.h"

#include <memory>

extern ECSManager ECS;
extern GameManager GameSceneManager;

namespace TestEnvironment
{
    namespace
    {
        SceneEditorScene* g_Editor = nullptr;
        ScenePlayer* g_Player = nullptr;
    } // namespace

    void Init()
    {
        if (g_Editor != nullptr)
            return;
        ECS.Init();
        GameSceneManager.Setup();
        RegisterGameScripts();

        auto player = std::make_unique<ScenePlayer>();
        g_Player = player.get();
        player->OnExit = [] { GameSceneManager.RequestSceneChange(SceneMenu::NAME); };
        GameSceneManager.RegisterScene(ScenePlayer::NAME, std::move(player));
        auto editor = std::make_unique<SceneEditorScene>();
        g_Editor = editor.get();
        GameSceneManager.RegisterScene(EDITOR_SCENE, std::move(editor));
        GameSceneManager.RegisterScene(SceneMenu::NAME, std::make_unique<SceneMenu>());
        GameSceneManager.SetActiveScene(ScenePlayer::NAME);
    }

    SceneEditorScene& Editor() { return *g_Editor; }

    ScenePlayer& Player() { return *g_Player; }

    void RunFrame(float deltaMilliseconds)
    {
        // Text and lines are recorded per frame, the mouse / keys persist
        AppStub::Get().Printed.clear();
        AppStub::Get().LinesDrawn = 0;
        AppStub::Get().Lines.clear();
        AppStub::Get().Triangles.clear();
        GameSceneManager.Update(deltaMilliseconds);
        GameSceneManager.Render();
        ECS.FlushECS();
    }

    void RunFrames(int count, float deltaMilliseconds)
    {
        for (int i = 0; i < count; ++i)
            RunFrame(deltaMilliseconds);
    }
} // namespace TestEnvironment
