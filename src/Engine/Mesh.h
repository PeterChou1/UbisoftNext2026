//---------------------------------------------------------------------------------
// Mesh.h
//---------------------------------------------------------------------------------
//
// Component rendering a 3D model loaded from data/models/<Model>.obj through
// the rendering system
//
#pragma once

#include <string>

struct Mesh
{
    // Set by the MeshHandler once the geometry was added to the render buffers
    bool Loaded{};
    // Model name (file name without extension in data/models)
    std::string Model;

    Mesh() = default;

    explicit Mesh(std::string model)
        : Model(std::move(model))
    {
    }
};
