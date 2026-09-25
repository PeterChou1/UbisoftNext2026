//---------------------------------------------------------------------------------
// Material.h
//---------------------------------------------------------------------------------
//
// A Material is a class representing the loaded .mtl material of an .obj object
// Depending on the material it may contain a texture or not
//
#pragma once
#include "SIMD.h"
#include "Vec3.h"

class Material
{
  public:
    Material() = default;

    /**
     * \brief Loads a Material without a texture
     */
    Material(Vec3 ambient, Vec3 diffuse, Vec3 specular, float highlight);

    /**
     * \brief Samples 8 pixels at once
     * \param tex UV coordinates of the 8 pixels
     * \param r output red channel
     * \param g output green channel
     * \param b output blue channel
     */
    void SampleSIMD(SIMDFloat& r, SIMDFloat& g, SIMDFloat& b) const;

    Vec3 ambient{};
    Vec3 diffuse{};
    Vec3 specular{};
    float highlight{};
    bool hasTexture;

    // Default Material used if a .obj model has no texture information
    static Material DefaultMaterial;
};
