#include "Material.h"

#include "stdafx.h"

Material Material::DefaultMaterial = Material({0, 0, 0}, {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, 10.0);

Material::Material(Vec3 ambient, Vec3 diffuse, Vec3 specular, float highlight)
    : ambient(ambient)
    , diffuse(diffuse)
    , specular(specular)
    , highlight(highlight)
    , hasTexture(false)
{
}

void Material::ResetMaterial()
{
    diffuse = Vec3(0.0f, 0.0f, 0.0f);
    ambient = Vec3(0.0f, 0.0f, 0.0f);
    specular = Vec3(0.0f, 0.0f, 0.0f);
}

void Material::SampleSIMD(SIMDFloat& r, SIMDFloat& g, SIMDFloat& b) const
{
    r = 255 * diffuse[0];
    g = 255 * diffuse[1];
    b = 255 * diffuse[2];
}
