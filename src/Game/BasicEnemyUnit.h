#pragma once

#include "Entity.h"

struct BasicEnemyUnit
{
    int health;
};

Entity CreateEnemyTank(float x, float y, int health);

Entity CreateShootEnemyUnit(float x, float y, int health, float speed);

void CreateEnemyBattalion(float x, float y, int health, int unitcount, float speed);