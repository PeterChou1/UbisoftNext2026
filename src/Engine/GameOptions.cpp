#include "GameOptions.h"

#include "stdafx.h"

void GameOptions::SetGameOptions(GraphicsOptions options)
{
    switch (options)
    {
    case PotatoMode: {
        LineRendering = true;
        ShadowMapping = false;
        renderingType = 0;
        shadowQuality = 0;
        return;
    }
    case Low: {
        LineRendering = false;
        ShadowMapping = false;
        renderingType = 0;
        shadowQuality = 0;
        return;
    }
    case Medium: {
        LineRendering = false;
        ShadowMapping = true;
        renderingType = 0;
        shadowQuality = 2;
        return;
    }
    case High: {
        LineRendering = false;
        ShadowMapping = true;
        renderingType = 0;
        shadowQuality = 3;
    }
    }
}
