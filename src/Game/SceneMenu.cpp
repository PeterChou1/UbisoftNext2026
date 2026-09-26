#include "SceneMenu.h"

#include "ECSManager.h"
#include "GameManager.h"
#include "UIState.h"
#include "Widget.h"
#include "stdafx.h"

#include <algorithm>
#include <filesystem>

extern ECSManager ECS;
extern GameManager GameSceneManager;

namespace
{
    constexpr int PER_PAGE = 8;
    constexpr float BUTTON_W = 320.0f;
    constexpr float BUTTON_H = 40.0f;
} // namespace

void SceneMenu::Setup()
{
    m_Scenes.clear();
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(GameManager::SCENES_DIRECTORY, ec))
    {
        if (entry.is_regular_file() && entry.path().extension() == GameManager::SCENE_EXTENSION)
            m_Scenes.push_back(entry.path().stem().string());
    }
    std::sort(m_Scenes.begin(), m_Scenes.end());
    m_Page = 0;
}

void SceneMenu::Render()
{
    UIState& ui = *ECS.GetResource<UIState>();
    float cx = APP_VIRTUAL_WIDTH * 0.5f;
    float y = APP_VIRTUAL_HEIGHT - 110.0f;
    App::Print(cx - 60.0f, y + 40.0f, "PICK A SCENE", 1.0f, 0.82f, 0.25f);
    if (m_Scenes.empty())
    {
        App::Print(cx - 170.0f, y, "No scenes in data/scenes, build some with the SceneEditor");
        return;
    }

    int first = m_Page * PER_PAGE;
    int last = std::min(first + PER_PAGE, static_cast<int>(m_Scenes.size()));
    for (int i = first; i < last; ++i)
    {
        y -= BUTTON_H + 12.0f;
        if (Button(10 + i, cx - BUTTON_W * 0.5f, y, ui, BUTTON_W, BUTTON_H, m_Scenes[i]))
        {
            GameSceneManager.RequestLoad(GameManager::ScenePath(m_Scenes[i]));
            return;
        }
    }

    int pages = (static_cast<int>(m_Scenes.size()) + PER_PAGE - 1) / PER_PAGE;
    if (pages > 1)
    {
        if (m_Page > 0 && Button(1, cx - 170.0f, 60.0f, ui, 100.0f, 36.0f, "Prev"))
            --m_Page;
        if (m_Page + 1 < pages && Button(2, cx + 70.0f, 60.0f, ui, 100.0f, 36.0f, "Next"))
            ++m_Page;
    }
    App::Print(20.0f, 20.0f, "Q quits", 0.6f, 0.6f, 0.6f);
}
