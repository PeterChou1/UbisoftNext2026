//---------------------------------------------------------------------------------
// UIUtilities.h
//---------------------------------------------------------------------------------
//
// UI utility for drawing various graphics primitives
//
#pragma once

#include "app.h"

void DrawRect(const float x,
              const float y,
              const float width,
              const float height,
              const float r,
              const float g,
              const float b)
{

    float scanY = 0.0f;
    while (scanY <= height)
    {
        App::DrawLine(x, y + scanY, x + width, y + scanY, r, g, b);
        scanY += 0.1f;
    }
}

int RegionHit(float mouseX, float mouseY, float x, float y, float width, float height)
{

    if (mouseX < x || mouseY < y || mouseX >= x + width || mouseY >= y + height)
        return 0;

    return 1;
}
