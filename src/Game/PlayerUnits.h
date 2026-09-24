#pragma once

#include "Entity.h"

struct PlayerControlUnit
{
    int battalionId;
    int health;
    bool selected;
    bool isTank = false;
    bool isWall = false;
};

void CreateSoldierBattalion(float x, float y, int health, int unitcount, int battlionId);

void CreateSupportBattalion(float x, float y, int health, int unitcount, int battlionId);

void CreateTank(float x, float y, int battalionId);

// Behaviour tree setup, split from the Create functions so the AI can be
// reattached to units restored from a save file
void AttachSoldierBehaviour(Entity Unit);

void AttachSupportBehaviour(Entity Unit);

void AttachPlayerTankBehaviour(Entity TankEntity, Entity TankCannonEntity);