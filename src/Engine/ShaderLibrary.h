//---------------------------------------------------------------------------------
// ShaderLibrary.h
//---------------------------------------------------------------------------------
//
// Names of the shaders a mesh or a shape can use, for the scene editor and for
// scripts: an object's shaders are its FragShaderTag (fragment / pixel shader)
// and VertShaderTag (vertex shader) components.
//
//   ShaderLibrary::FragmentShaders()   the fragment shaders offered in the editor
//   ShaderLibrary::Name(RimShaderID)   "Rim"
//   ShaderLibrary::Find("Wave", id)    the id of a vertex shader by name
//
#pragma once

#include "Assets.h"

#include <string>
#include <vector>

namespace ShaderLibrary
{
    template <typename Id>
    struct Entry
    {
        Id Value;
        const char* Name;
        const char* Description;
    };

    /**
     * \brief Fragment shaders that make sense on shapes and models (the
     *        internal ones, e.g. particles, are not listed)
     */
    const std::vector<Entry<FragShaderTypeID>>& FragmentShaders();

    /**
     * \brief Every vertex shader
     */
    const std::vector<Entry<VertShaderTypeID>>& VertexShaders();

    std::string Name(FragShaderTypeID id);
    std::string Name(VertShaderTypeID id);

    /**
     * \brief Look a shader up by name (case sensitive), false if unknown
     */
    bool Find(const std::string& name, FragShaderTypeID& id);
    bool Find(const std::string& name, VertShaderTypeID& id);
} // namespace ShaderLibrary
