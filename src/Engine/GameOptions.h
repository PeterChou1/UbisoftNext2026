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
    // false: the engine's software rasterizer (every fragment shader, shadow
    // maps). true: hardware triangles, shaded at their corners (faster). Tab
    // switches
    bool LineRendering = false;
    bool Projection = true;
    // Shadows allowed (a quality setting). They are drawn when the scene's
    // light casts them too (LightShadows) and with the software rasterizer
    bool ShadowMapping = true;
    // Set every frame from the scene's light object (SceneLight.h)
    bool LightShadows = true;
    int renderingType = 0;
    int renderingProjection = 0;
    int shadowQuality = 0;
    int GameResolution = 0;
    int VirtualWidth = APP_VIRTUAL_WIDTH;
    int VirtualHeight = APP_VIRTUAL_HEIGHT;
    float ScreenRatio = APP_VIRTUAL_WIDTH / APP_VIRTUAL_HEIGHT;

    void SetGameOptions(GraphicsOptions options);

    /**
     * \brief Shadow maps are drawn and sampled this frame
     */
    bool ShadowsOn() const { return ShadowMapping && LightShadows && !LineRendering; }

    void ResetResource() override {}
};
