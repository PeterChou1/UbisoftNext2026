//---------------------------------------------------------------------------------
// CollectGame.h
//---------------------------------------------------------------------------------
//
// Scene script with simple "collect everything" rules:
//   - every object tagged "Pickup" must be collected (see Collectible)
//   - hazards cost a life (see Hazard / Projectile), the player respawns at its
//     start position and is briefly invulnerable
//   - when every pickup is collected the next level (data/scenes/level_<N+1>)
//     is loaded, or "You win" is shown after the last one
//   - with no lives left the level restarts
//
// Parameters: Level (number of this level), Lives
//
#pragma once

#include "Scripting/Script.h"

class CollectGame : public SceneScript
{
  public:
    enum class State
    {
        Playing,
        LevelComplete,
        GameOver,
        Finished
    };

    void OnStart() override;
    void OnUpdate(float deltaSeconds) override;
    void OnRender() override;

    void AddScore(int points);
    void PlayerHit();

    int Score() const { return m_Score; }
    int Lives() const { return m_Lives; }
    int PickupsLeft() const;
    State GetState() const { return m_State; }

  private:
    int m_Score = 0;
    int m_Lives = 3;
    float m_Time = 0.0f;
    float m_StateTimer = 0.0f;
    float m_Invulnerable = 0.0f;
    State m_State = State::Playing;
    Entity m_Player = NULL_ENTITY;
    Vec3 m_PlayerStart;
};
