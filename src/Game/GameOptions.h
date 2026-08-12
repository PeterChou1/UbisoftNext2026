#pragma once
#include "AppSettings.h"
#include "Resource.h"
#include "Transform.h"

enum GraphicsOptions
{
    PotatoMode,
    Low,
    Medium,
    High
};

class GameOptions : public Resource
{
  public:
    GraphicsOptions Options;
    bool LineRendering = true;
    bool Projection = true;
    bool ShadowMapping = false;
    int renderingType = 0;
    int renderingProjection = 0;
    int shadowQuality = 0;
    int GameResolution = 0;
    int VirtualWidth = APP_VIRTUAL_WIDTH;
    int VirtualHeight = APP_VIRTUAL_HEIGHT;
    float ScreenRatio = APP_VIRTUAL_WIDTH / APP_VIRTUAL_HEIGHT;

    void SetGameOptions(GraphicsOptions options);

    void ResetResource() override {}
};
