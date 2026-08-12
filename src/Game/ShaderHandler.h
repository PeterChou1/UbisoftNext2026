//---------------------------------------------------------------------------------
// ShaderHandler.h
//---------------------------------------------------------------------------------
//
// System to update all Shaders
//

#pragma once
#include "GameOptions.h"
#include "RenderConstants.h"

#include <memory>

class ShaderHandler
{
  public:
    ShaderHandler();

    void Update(float deltaTime);

    void HandleShaderDelete();

  private:
    std::shared_ptr<GameOptions> m_Options;
    std::shared_ptr<RenderConstants> m_Constants;
};
