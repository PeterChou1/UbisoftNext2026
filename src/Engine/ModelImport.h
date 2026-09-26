//---------------------------------------------------------------------------------
// ModelImport.h
//---------------------------------------------------------------------------------
//
// Brings a custom Wavefront .obj model into the project: the .obj and the
// .mtl material files it uses are copied into data/models, after which the
// model is available everywhere by name (SceneObjects::CreateModel, the
// editor's Model palette entry, the Mesh component).
//
//   ModelImport::Result result = ModelImport::Import("C:/art/tree.obj");
//   if (result) editor.Place(ObjectKind::Model, at, {.Model = result.Name});
//
// The file is checked before anything is copied (it must load and have
// faces). Importing the same file twice reuses the first copy; a different
// file with a name already used gets a numbered name (tree_2).
//
#pragma once

#include <string>
#include <vector>

namespace ModelImport
{
    struct Result
    {
        bool Success = false;
        // Model name to use in scenes (the file name in data/models without .obj)
        std::string Name;
        std::string Error;
        size_t Vertices = 0;
        size_t Triangles = 0;
        // True when an identical copy was already imported
        bool AlreadyImported = false;
        // Problems that did not stop the import (missing .mtl ...)
        std::vector<std::string> Warnings;

        explicit operator bool() const { return Success; }
    };

    /**
     * \param sourcePath .obj file (quotes around it and a leading ~/ are accepted)
     * \param modelsDirectory where models live (default data/models/)
     */
    Result Import(const std::string& sourcePath, const std::string& modelsDirectory = "data/models/");
} // namespace ModelImport
