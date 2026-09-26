//---------------------------------------------------------------------------------
// AuthorScenes.cpp
//---------------------------------------------------------------------------------
//
// Writes every sample scene (tests/scenes/SampleScenes.cpp) to a directory:
//
//     AuthorScenes <scene directory> [prefab directory]
//
// The sample prefabs are written as plain text to the prefab directory
// (default: data/prefabs next to the scene directory).
//
// Run through the `author_scenes` CMake target to refresh data/scenes. The
// scenes use models from data/models, so run it from the repository root (the
// CMake target does)
//
#include "SampleScenes.h"
#include "TestEnvironment.h"

#include <iostream>

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: AuthorScenes <scene directory> [prefab directory]\n";
        return 2;
    }
    std::string directory = argv[1];
    TestEnvironment::Init();
    Editor::SceneEditor editor;

    int failures = 0;
    for (const auto& scene : SampleScenes::All())
    {
        scene.Author(editor);
        // Some scenes are committed as plain text
        Serialization::WorldSerializer::SetFileFormat(SampleScenes::FormatOf(scene));
        std::string path = directory + "/" + scene.Name + ".ubsave";
        Serialization::SaveResult result = editor.SaveScene(path, scene.Name);
        if (result)
        {
            std::cout << "wrote " << path << " (" << result.BytesWritten << " bytes, "
                      << editor.Objects().size() << " objects)\n";
        }
        else
        {
            std::cerr << "failed to write " << path << ": " << result.Error << "\n";
            ++failures;
        }
    }
    std::string prefabDirectory = argc > 2 ? argv[2] : directory + "/../prefabs";
    Serialization::WorldSerializer::SetFileFormat(Serialization::SaveFormat::Text);
    for (const auto& prefab : SampleScenes::Prefabs())
    {
        Prefab::Data data = prefab.Author(editor);
        std::string path = Prefab::PathOf(prefab.Name, prefabDirectory);
        std::string error;
        if (Prefab::SaveFile(path, data, error))
            std::cout << "wrote " << path << " (" << data.Objects.size() << " objects)\n";
        else
        {
            std::cerr << "failed to write " << path << ": " << error << "\n";
            ++failures;
        }
    }
    Serialization::WorldSerializer::SetFileFormat(Serialization::SaveFormat::Binary);
    return failures == 0 ? 0 : 1;
}
