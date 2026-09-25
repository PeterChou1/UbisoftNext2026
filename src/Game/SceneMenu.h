//---------------------------------------------------------------------------------
// SceneMenu.h
//---------------------------------------------------------------------------------
//
// Start screen of the Game: lists the scenes in data/scenes and plays the one
// clicked (GameManager::RequestLoad -> the engine's ScenePlayer)
//
#pragma once

#include "Scene.h"

#include <string>
#include <vector>

class SceneMenu : public Scene
{
  public:
    static constexpr const char* NAME = "Menu";

    void Setup() override;
    void Render() override;

  private:
    std::vector<std::string> m_Scenes;
    int m_Page = 0;
};
