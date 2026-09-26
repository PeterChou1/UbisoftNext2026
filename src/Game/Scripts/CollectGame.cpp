#include "CollectGame.h"

#include "GameManager.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace
{
    // Seconds a message stays before the next level / restart
    constexpr float TRANSITION_TIME = 2.0f;
    // Seconds of invulnerability after being hit
    constexpr float HIT_COOLDOWN = 1.0f;
} // namespace

void CollectGame::OnStart()
{
    m_Lives = std::max(1, static_cast<int>(Param("Lives")));
    std::vector<Entity> players = FindByTag("Player");
    if (!players.empty())
    {
        m_Player = players.front();
        m_PlayerStart = PositionOf(m_Player);
    }
}

int CollectGame::PickupsLeft() const
{
    return static_cast<int>(FindByTag("Pickup").size());
}

void CollectGame::AddScore(int points)
{
    if (m_State == State::Playing)
        m_Score += points;
}

void CollectGame::PlayerHit()
{
    if (m_State != State::Playing || m_Invulnerable > 0.0f)
        return;
    --m_Lives;
    m_Invulnerable = HIT_COOLDOWN;
    if (m_Lives <= 0)
    {
        m_State = State::GameOver;
        m_StateTimer = TRANSITION_TIME;
        return;
    }
    if (IsAlive(m_Player))
        SetPositionOf(m_Player, m_PlayerStart);
}

void CollectGame::OnUpdate(float deltaSeconds)
{
    m_Invulnerable = std::max(0.0f, m_Invulnerable - deltaSeconds);
    switch (m_State)
    {
    case State::Playing:
        m_Time += deltaSeconds;
        if (PickupsLeft() == 0)
        {
            m_State = State::LevelComplete;
            m_StateTimer = TRANSITION_TIME;
        }
        break;
    case State::LevelComplete: {
        m_StateTimer -= deltaSeconds;
        if (m_StateTimer > 0.0f)
            break;
        std::string next = "level_" + std::to_string(static_cast<int>(Param("Level")) + 1);
        if (std::filesystem::exists(GameManager::ScenePath(next)))
            LoadScene(next);
        else
            m_State = State::Finished;
        break;
    }
    case State::GameOver:
        m_StateTimer -= deltaSeconds;
        if (m_StateTimer <= 0.0f)
            RestartScene();
        break;
    case State::Finished:
        break;
    }
}

void CollectGame::OnRender()
{
    char hud[128];
    std::snprintf(hud,
                  sizeof(hud),
                  "Level %d   Score %d   Lives %d   Pickups left %d   Time %.0f",
                  static_cast<int>(Param("Level")),
                  m_Score,
                  m_Lives,
                  PickupsLeft(),
                  m_Time);
    DrawText(20.0f, APP_VIRTUAL_HEIGHT - 70.0f, hud);

    float cx = APP_VIRTUAL_WIDTH * 0.5f - 80.0f;
    float cy = APP_VIRTUAL_HEIGHT * 0.5f;
    if (m_State == State::LevelComplete)
        DrawText(cx, cy, "Level complete!", {0.4f, 1.0f, 0.4f});
    else if (m_State == State::GameOver)
        DrawText(cx, cy, "Out of lives...", {1.0f, 0.4f, 0.3f});
    else if (m_State == State::Finished)
        DrawText(cx, cy, "You win! Esc for the menu", {1.0f, 0.85f, 0.3f});
}
