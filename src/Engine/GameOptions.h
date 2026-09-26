#pragma once
#include "Resource.h"

class GameOptions : public Resource
{
  public:
    // false: the engine's software rasterizer (every fragment shader, shadow
    // maps). true: hardware triangles, shaded at their corners (faster). Tab
    // switches
    bool LineRendering = false;
    // Shadows allowed (a quality setting). They are drawn when the scene's
    // light casts them too (LightShadows) and with the software rasterizer
    bool ShadowMapping = true;
    // Set every frame from the scene's light object (SceneLight.h)
    bool LightShadows = true;

    /**
     * \brief Shadow maps are drawn and sampled this frame
     */
    bool ShadowsOn() const { return ShadowMapping && LightShadows && !LineRendering; }

    void ResetResource() override {}
};
