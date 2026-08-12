//---------------------------------------------------------------------------------
// SIMDShader.h
//---------------------------------------------------------------------------------
//
// Abstract base class for all shaders
// faster than conventional shading as
// SIMD (AVX2) allows us to shade 8 pixels at once
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
    bool ShadowMapping;
    float DeltaTime = 0;

    virtual ~FragmentShader() = default;

    /**
     * \brief Abstract Shade Method called during fragment shading
     * \param pixel The sets of 8 pixels to shade
     * \param lights All lights in the scene
     * \param texture The texture belonging to the pixels
     * \param camera The current camera in the scene
     */
    virtual void Shade(SIMDPixel& pixel,
                       DepthBuffer& depthBuffer,
                       Material& texture,
                       Camera& camera,
                       DirectionalLight& Light) = 0;
};
