//---------------------------------------------------------------------------------
// Material.h
//---------------------------------------------------------------------------------
//
// A Material is a class representing the loaded .mtl material of an .obj object
//
#pragma once
#include "SIMD.h"
#include "Vec3.h"

class Material
{
  public:
    Material() = default;

    Material(Vec3 ambient, Vec3 diffuse, Vec3 specular, float highlight)
        : ambient(ambient)
        , diffuse(diffuse)
        , specular(specular)
        , highlight(highlight)
    {
    }

    /// Colour of 8 pixels at once (0..255 per channel): the diffuse colour
    void SampleSIMD(SIMDFloat& r, SIMDFloat& g, SIMDFloat& b) const;

    Vec3 ambient{};
    Vec3 diffuse{};
    Vec3 specular{};
    float highlight{};

    // Default Material used if a .obj model has no texture information
    static Material DefaultMaterial;
};
