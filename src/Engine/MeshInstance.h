//---------------------------------------------------------------------------------
// MeshInstance.h
//---------------------------------------------------------------------------------
//
// A MeshInstance is a wrapper class for the loaded geometry of a .obj file
//
#pragma once

#include "Entity.h"
#include "Transform.h"
#include "Vertex.h"

#include <vector>

struct MeshInstance
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;

    void transform(const Transform& t)
    {
        for (Vertex& v : vertices)
        {
            v.Position = t.TransformVec3(v.LocalPosition);
            v.Normal = t.TransformNormal(v.LocalNormal);
        }
    }

};
