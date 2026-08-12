#pragma once

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