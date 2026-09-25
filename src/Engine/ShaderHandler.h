//---------------------------------------------------------------------------------
// ShaderHandler.h
//---------------------------------------------------------------------------------
//
// Gives entities with a FragShaderTag / VertShaderTag their shader instances
// and advances every shader's time
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
