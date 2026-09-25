//---------------------------------------------------------------------------------
// FragmentShader.h
//---------------------------------------------------------------------------------
//
// Abstract base class for all fragment shaders: SIMD shades 8 pixels at once
//
#pragma once
#include "Camera.h"
#include "DepthBuffer.h"
#include "Lights.h"
#include "Material.h"
#include "SIMDPixel.h"

class FragmentShader
{
  public:
    bool ShadowMapping = false;
    // Time the shader has run (seconds), advanced by the ShaderHandler
    float DeltaTime = 0;

    virtual ~FragmentShader() = default;

    /**
     * \brief Set pixel.Color of the 8 pixels
     * \param texture The material of the pixels' triangle
     */
    virtual void Shade(SIMDPixel& pixel,
                       DepthBuffer& depthBuffer,
                       Material& texture,
                       Camera& camera,
                       DirectionalLight& light) = 0;
};
