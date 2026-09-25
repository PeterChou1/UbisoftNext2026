#include "Material.h"

Material Material::DefaultMaterial = Material({0, 0, 0}, {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, 10.0);

void Material::SampleSIMD(SIMDFloat& r, SIMDFloat& g, SIMDFloat& b) const
{
    r = 255 * diffuse[0];
    g = 255 * diffuse[1];
    b = 255 * diffuse[2];
}
