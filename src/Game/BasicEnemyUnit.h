#pragma once

#include "Entity.h"

struct BasicEnemyUnit
{
    int health;
    // Walking speed of the unit, stored on the component so the behaviour tree
    // can be rebuilt with the same speed after loading a save
    float Speed = 0.0f;
};

Entity CreateEnemyTank(float x, float y, int health);

Entity CreateShootEnemyUnit(float x, float y, int health, float speed);

void CreateEnemyBattalion(float x, float y, int health, int unitcount, float speed);

// Behaviour tree setup, split from the Create functions so the AI can be
// reattached to units restored from a save file
void AttachShootEnemyBehaviour(Entity Unit, float speed);

void AttachEnemyTankBehaviour(Entity TankEntity, Entity TankCannonEntity);